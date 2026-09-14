#include "Core/stdafx.h"
#include "Terrain/TessellatedTerrainRenderer.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Core/Graphics.h"
#include "Graphics/Vertex.h"
#include "Graphics/ComputeShader.h"
#include "Utils/Paths.h"

#include <chrono>

#include <limits>

using namespace DirectX;

namespace
{
    // HeightGenCS.hlsl 의 cbuffer 와 같은 배치 : 16바이트씩 3줄 = 48바이트
    struct HeightGenConstants
    {
        uint32_t seed;
        uint32_t octaves;
        uint32_t noiseType;
        uint32_t size;

        float frequency;
        float amplitude;
        float persistence;
        float lacunarity;

        float worldWidth;
        float worldDepth;
        float flatten;
        float padding;
    };

    constexpr int kHeightMapSizes[] = { 256, 512, 1024, 2048 };
}

void TessellatedTerrainRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;

    // 컴퓨트 셰이더 준비 (S71). 실패해도 CPU 경로로 계속 동작한다.
    if (m_graphics && m_graphics->GetDevice())
    {
        m_heightGen = std::make_shared<ComputeShader>();
        if (!m_heightGen->Load(m_graphics->GetDevice(), Paths::Resolve(L"Shaders/HeightGenCS.hlsl")))
            dxutil::DebugLog(L"[Tess] 컴퓨트 셰이더를 쓸 수 없어 CPU 로 만든다 : %s", m_heightGen->GetLastError().c_str());

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth      = sizeof(HeightGenConstants);
        cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        DX_FAILED(m_graphics->GetDevice()->CreateBuffer(&cbDesc, nullptr, m_genConstants.GetAddressOf()),
                  L"CreateBuffer(height gen constants)");
    }

    BuildHeightTexture();
    BuildPatchGrid();
    m_dirty = false;
}

void TessellatedTerrainRenderer::OnDestroy()
{
    m_patches.Release();
    m_heightSRV.Reset();
    m_heightUAV.Reset();
    m_heightTexture.Reset();
    m_stagingTexel.Reset();
    m_stagingFull.Reset();
    m_genConstants.Reset();
    m_heightGen.reset();
    m_heightTextureSize = 0;
    m_graphics = nullptr;
}

void TessellatedTerrainRenderer::SetGrid(int patchesX, int patchesZ, float patchSize)
{
    m_patchesX = patchesX;
    m_patchesZ = patchesZ;
    m_patchSize = patchSize;
    m_dirty = true;
}

void TessellatedTerrainRenderer::SetHeightParams(const terrain::HeightParams& params)
{
    m_height.SetParams(params);
    m_dirty = true;
}

void TessellatedTerrainRenderer::Regenerate(unsigned seed)
{
    terrain::HeightParams params = m_height.GetParams();
    params.seed = seed;
    m_height.SetParams(params);
    m_dirty = true;
}

void TessellatedTerrainRenderer::SetTessellationRange(float minFactor, float maxFactor)
{
    m_minFactor = (std::max)(1.0f, minFactor);
    m_maxFactor = (std::max)(m_minFactor, maxFactor);
}

void TessellatedTerrainRenderer::AdjustMaxFactor(float delta)
{
    // D3D11 의 분할 계수 상한은 64 다.
    m_maxFactor = (std::max)(m_minFactor, (std::min)(64.0f, m_maxFactor + delta));
}

// -------------------------------------------------------------
// 높이맵 텍스처 (S62)
//  CPU 의 노이즈를 한 번 구워 GPU 텍스처로 올린다.
//  도메인 셰이더가 만들어 낸 정점의 높이를 여기서 읽는다.
//  R32_FLOAT 한 채널이면 충분하고, 8비트보다 계단이 생기지 않는다.
// -------------------------------------------------------------
bool TessellatedTerrainRenderer::BuildHeightTexture()
{
    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    if (!EnsureHeightTexture(m_heightMapSize))
        return false;

    // GPU 로 만들 수 없으면(컴파일 실패 · 기능 수준 부족) CPU 로 되돌린다.
    if (m_useGpuGeneration && GenerateOnGpu())
        return true;

    GenerateOnCpu();
    return true;
}

bool TessellatedTerrainRenderer::IsGpuGenerationAvailable() const
{
    return m_heightGen && m_heightGen->IsValid() && m_genConstants && m_heightUAV;
}

void TessellatedTerrainRenderer::CycleHeightMapSize()
{
    const int count = static_cast<int>(sizeof(kHeightMapSizes) / sizeof(kHeightMapSizes[0]));

    int next = kHeightMapSizes[0];
    for (int i = 0; i < count; ++i)
    {
        if (kHeightMapSizes[i] == m_heightMapSize)
        {
            next = kHeightMapSizes[(i + 1) % count];
            break;
        }
    }

    m_heightMapSize = next;
    m_dirty = true;
}

// -------------------------------------------------------------
// 높이 텍스처 준비
//  이 텍스처는 컴퓨트 셰이더가 쓰고(UAV) 도메인 셰이더가 읽는다(SRV).
//  IMMUTABLE 은 만든 뒤 아무도 쓸 수 없으므로 DEFAULT 로 만들고 뷰를 두 개 붙인다.
// -------------------------------------------------------------
bool TessellatedTerrainRenderer::EnsureHeightTexture(int size)
{
    if (m_heightTexture && m_heightTextureSize == size)
        return true;

    ID3D11Device* device = m_graphics->GetDevice();

    m_heightSRV.Reset();
    m_heightUAV.Reset();
    m_heightTexture.Reset();
    m_stagingTexel.Reset();
    m_stagingFull.Reset();
    m_heightTextureSize = 0;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = static_cast<UINT>(size);
    desc.Height           = static_cast<UINT>(size);
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    if (DX_FAILED(device->CreateTexture2D(&desc, nullptr, m_heightTexture.GetAddressOf()),
                  L"CreateTexture2D(heightmap)"))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format              = desc.Format;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (DX_FAILED(device->CreateShaderResourceView(m_heightTexture.Get(), &srvDesc, m_heightSRV.GetAddressOf()),
                  L"CreateShaderResourceView(heightmap)"))
        return false;

    // UAV 는 없어도 된다. 없으면 GPU 생성만 못 쓴다.
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format             = desc.Format;
    uavDesc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    if (FAILED(device->CreateUnorderedAccessView(m_heightTexture.Get(), &uavDesc, m_heightUAV.GetAddressOf())))
        m_heightUAV.Reset();

    // 읽어 오기용 스테이징 텍스처 : GPU 에 묶이지 않고 CPU 가 Map 할 수 있는 복사본 (S72)
    D3D11_TEXTURE2D_DESC staging = desc;
    staging.Usage          = D3D11_USAGE_STAGING;
    staging.BindFlags      = 0;
    staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    if (DX_FAILED(device->CreateTexture2D(&staging, nullptr, m_stagingFull.GetAddressOf()),
                  L"CreateTexture2D(staging full)"))
        return false;

    staging.Width = 1;
    staging.Height = 1;
    if (DX_FAILED(device->CreateTexture2D(&staging, nullptr, m_stagingTexel.GetAddressOf()),
                  L"CreateTexture2D(staging texel)"))
        return false;

    m_heightTextureSize = size;
    return true;
}

// -------------------------------------------------------------
// CPU 생성 (스텝 7 의 원래 방식)
//  CPU 의 노이즈를 텍셀마다 한 번씩 부르고, 결과를 GPU 로 올린다(업로드 포함해서 잰다).
// -------------------------------------------------------------
void TessellatedTerrainRenderer::GenerateOnCpu()
{
    const auto start = std::chrono::steady_clock::now();

    const int size = m_heightMapSize;
    const float worldWidth = m_patchesX * m_patchSize;
    const float worldDepth = m_patchesZ * m_patchSize;

    std::vector<float> heights(static_cast<size_t>(size) * size, 0.0f);

    m_minHeight = (std::numeric_limits<float>::max)();
    m_maxHeight = -(std::numeric_limits<float>::max)();

    for (int y = 0; y < size; ++y)
    {
        // v = 0 이 +Z 가 되도록 맞춘다 (격자 생성 규약과 동일)
        const float v = static_cast<float>(y) / (size - 1);
        const float z = worldDepth * 0.5f - v * worldDepth;

        for (int x = 0; x < size; ++x)
        {
            const float u = static_cast<float>(x) / (size - 1);
            const float worldX = -worldWidth * 0.5f + u * worldWidth;

            const float h = m_height.Sample(worldX, z);
            heights[static_cast<size_t>(y) * size + x] = h;

            m_minHeight = (std::min)(m_minHeight, h);
            m_maxHeight = (std::max)(m_maxHeight, h);
        }
    }

    m_graphics->GetContext()->UpdateSubresource(m_heightTexture.Get(), 0, nullptr, heights.data(),
                                                static_cast<UINT>(sizeof(float) * size), 0);

    const auto end = std::chrono::steady_clock::now();
    m_lastCpuMs = std::chrono::duration<float, std::milli>(end - start).count();

    if (m_maxHeight <= m_minHeight)
    {
        m_minHeight = 0.0f;
        m_maxHeight = 1.0f;
    }

    dxutil::DebugLog(L"[Tess] CPU 로 높이맵 %d x %d : %.1f ms (높이 %.1f ~ %.1f)",
                     size, size, m_lastCpuMs, m_minHeight, m_maxHeight);
}

// -------------------------------------------------------------
// GPU 생성 (S71, S72)
// -------------------------------------------------------------
bool TessellatedTerrainRenderer::GenerateOnGpu()
{
    ID3D11DeviceContext* context = m_graphics->GetContext();
    if (!context || !IsGpuGenerationAvailable())
        return false;

    const int size = m_heightMapSize;
    const terrain::HeightParams& params = m_height.GetParams();

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_genConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    HeightGenConstants* cb = static_cast<HeightGenConstants*>(mapped.pData);
    cb->seed        = params.seed;
    cb->octaves     = static_cast<uint32_t>((std::max)(1, params.octaves));
    cb->noiseType   = (params.noiseType == terrain::NoiseType::Perlin) ? 1u : 0u;
    cb->size        = static_cast<uint32_t>(size);
    cb->frequency   = params.frequency;
    cb->amplitude   = params.amplitude;
    cb->persistence = params.persistence;
    cb->lacunarity  = params.lacunarity;
    cb->worldWidth  = m_patchesX * m_patchSize;
    cb->worldDepth  = m_patchesZ * m_patchSize;
    cb->flatten     = params.flatten;
    cb->padding     = 0.0f;
    context->Unmap(m_genConstants.Get(), 0);

    auto dispatchAndWait = [&]()
    {
        // 1) 결과를 쓸 텍스처를 UAV(u0) 로 붙이고 셰이더를 건다.
        m_heightGen->Bind(context);
        ID3D11UnorderedAccessView* uav = m_heightUAV.Get();
        context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        context->CSSetConstantBuffers(0, 1, m_genConstants.GetAddressOf());

        // 2) 그룹 수 = 올림(size / 16). 셰이더의 [numthreads(16, 16, 1)] 과 곱해 텍스처 전체를 덮는다.
        const UINT groups = static_cast<UINT>((size + 15) / 16);
        context->Dispatch(groups, groups, 1);

        // 3) UAV 를 떼어 낸다. 같은 텍스처를 도메인 셰이더가 SRV 로 읽어야 하는데,
        //    한 리소스가 쓰기(UAV)와 읽기(SRV)에 동시에 붙어 있으면 D3D 가 읽기 쪽을 강제로 해제해 버린다.
        ID3D11UnorderedAccessView* nullUav = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
        context->CSSetShader(nullptr, nullptr, 0);

        // 4) 측정용 동기화 (S72)
        //    Dispatch 는 명령을 줄에 올리고 곧바로 돌아온다. 이 시점에는 아직 계산 전이다.
        //    텍셀 하나를 스테이징으로 복사해 Map 하면, 그 복사가 끝날 때까지(= 계산이 끝날 때까지) 기다린다.
        D3D11_BOX texelBox = { 0, 0, 0, 1, 1, 1 };
        context->CopySubresourceRegion(m_stagingTexel.Get(), 0, 0, 0, 0, m_heightTexture.Get(), 0, &texelBox);

        D3D11_MAPPED_SUBRESOURCE texel = {};
        if (SUCCEEDED(context->Map(m_stagingTexel.Get(), 0, D3D11_MAP_READ, 0, &texel)))
            context->Unmap(m_stagingTexel.Get(), 0);
    };

    // 첫 Dispatch 에는 드라이버가 셰이더 파이프라인을 준비하는 시간이 섞여 수십 ms 가 나온다.
    // 계산 자체의 시간을 보여 주려고 처음 한 번은 재지 않고 미리 돌려 둔다 (워밍업).
    if (!m_gpuWarmedUp)
    {
        dispatchAndWait();
        m_gpuWarmedUp = true;
    }

    const auto start = std::chrono::steady_clock::now();
    dispatchAndWait();
    const auto computed = std::chrono::steady_clock::now();
    m_lastGpuMs = std::chrono::duration<float, std::milli>(computed - start).count();

    // 5) 색상 램프에는 최저/최고 높이가 필요하다. 이건 CPU 가 알아야 하므로 전부 읽어 온다.
    //    계산은 GPU 에서 순식간이지만, 결과를 CPU 로 가져오는 복사가 따로 든다.
    context->CopyResource(m_stagingFull.Get(), m_heightTexture.Get());

    D3D11_MAPPED_SUBRESOURCE full = {};
    if (SUCCEEDED(context->Map(m_stagingFull.Get(), 0, D3D11_MAP_READ, 0, &full)))
    {
        m_minHeight = (std::numeric_limits<float>::max)();
        m_maxHeight = -(std::numeric_limits<float>::max)();

        // 한 줄의 실제 폭(RowPitch)은 텍스처 폭보다 클 수 있다. 반드시 RowPitch 로 건너뛴다.
        const UINT rowPitch = full.RowPitch / sizeof(float);
        const float* data = static_cast<const float*>(full.pData);

        for (int y = 0; y < size; ++y)
        {
            const float* row = data + static_cast<size_t>(y) * rowPitch;
            for (int x = 0; x < size; ++x)
            {
                m_minHeight = (std::min)(m_minHeight, row[x]);
                m_maxHeight = (std::max)(m_maxHeight, row[x]);
            }
        }

        context->Unmap(m_stagingFull.Get(), 0);
    }

    if (m_maxHeight <= m_minHeight)
    {
        m_minHeight = 0.0f;
        m_maxHeight = 1.0f;
    }

    const auto end = std::chrono::steady_clock::now();
    m_lastReadbackMs = std::chrono::duration<float, std::milli>(end - computed).count();

    dxutil::DebugLog(L"[Tess] GPU 로 높이맵 %d x %d : 계산 %.2f ms, 읽어 오기 %.2f ms (높이 %.1f ~ %.1f)",
                     size, size, m_lastGpuMs, m_lastReadbackMs, m_minHeight, m_maxHeight);
    return true;
}

// -------------------------------------------------------------
// 제어점 격자 (S59)
//  패치 하나당 제어점 4개. 정점 자체는 아주 성기다.
//  실제 삼각형은 GPU 가 만든다.
// -------------------------------------------------------------
bool TessellatedTerrainRenderer::BuildPatchGrid()
{
    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    const int cols = m_patchesX + 1;
    const int rows = m_patchesZ + 1;

    const float worldWidth = m_patchesX * m_patchSize;
    const float worldDepth = m_patchesZ * m_patchSize;

    std::vector<TerrainVertex> vertices;
    vertices.reserve(static_cast<size_t>(rows) * cols);

    for (int r = 0; r < rows; ++r)
    {
        const float v = static_cast<float>(r) / m_patchesZ;
        const float z = worldDepth * 0.5f - v * worldDepth;

        for (int c = 0; c < cols; ++c)
        {
            const float u = static_cast<float>(c) / m_patchesX;
            const float x = -worldWidth * 0.5f + u * worldWidth;

            TerrainVertex vertex{};
            vertex.position = XMFLOAT3(x, 0.0f, z);   // 높이는 도메인 셰이더가 올린다
            vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
            vertex.uv = XMFLOAT2(u, v);
            vertex.morphTargets = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

            vertices.push_back(vertex);
        }
    }

    // 패치마다 제어점 4개 : 좌상, 우상, 좌하, 우하
    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(m_patchesX) * m_patchesZ * 4);

    for (int r = 0; r < m_patchesZ; ++r)
    {
        for (int c = 0; c < m_patchesX; ++c)
        {
            const uint32_t topLeft     = static_cast<uint32_t>(r * cols + c);
            const uint32_t topRight    = topLeft + 1;
            const uint32_t bottomLeft  = static_cast<uint32_t>((r + 1) * cols + c);
            const uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    dxutil::DebugLog(L"[Tess] 패치 %d x %d (제어점 %zu개)",
                     m_patchesX, m_patchesZ, vertices.size());

    return m_patches.Create(m_graphics->GetDevice(),
                            vertices.data(), static_cast<UINT>(vertices.size()),
                            static_cast<UINT>(sizeof(TerrainVertex)),
                            indices.data(), static_cast<UINT>(indices.size()));
}

void TessellatedTerrainRenderer::Update()
{
    if (!m_dirty)
        return;

    m_dirty = false;
    BuildHeightTexture();
    BuildPatchGrid();
}

void TessellatedTerrainRenderer::Render()
{
    if (!m_graphics || !m_patches.IsValid() || !m_heightSRV)
        return;

    Transform* transform = GetTransform();
    if (!transform)
        return;

    Graphics::TessDrawParams draw;
    draw.tess = XMFLOAT4(m_minFactor, m_maxFactor, m_distanceRange, 1.0f);
    draw.heightRange = XMFLOAT4(m_minHeight, m_maxHeight, 0.0f, 0.0f);
    draw.params = XMFLOAT4(m_wireframe ? 1.0f : 0.0f,
                           m_patchesX * m_patchSize,
                           1.0f / static_cast<float>(m_heightMapSize),
                           0.0f);
    draw.heightMap = m_heightSRV.Get();
    draw.wireframe = m_wireframe;

    m_graphics->DrawTessellatedPatches(m_patches, transform->GetWorldMatrix(), draw);
}

// -------------------------------------------------------------
// 지면 높이 (S66)
//  높이 텍스처를 구운 것과 같은 함수에서 바로 읽는다.
//  GPU 가 몇 등분할지는 CPU 가 모르지만, 가까운 곳은 촘촘히 쪼개지므로 함수 값에 수렴한다.
// -------------------------------------------------------------
bool TessellatedTerrainRenderer::TryGetGroundHeight(float x, float z, float& outHeight) const
{
    XMFLOAT3 offset(0.0f, 0.0f, 0.0f);
    if (Transform* transform = GetTransform())
        offset = transform->GetWorldPosition();

    const float halfWidth = m_patchesX * m_patchSize * 0.5f;
    const float halfDepth = m_patchesZ * m_patchSize * 0.5f;

    const float localX = x - offset.x;
    const float localZ = z - offset.z;

    if (localX < -halfWidth || localX > halfWidth || localZ < -halfDepth || localZ > halfDepth)
        return false;

    outHeight = m_height.Sample(localX, localZ) + offset.y;
    return true;
}
