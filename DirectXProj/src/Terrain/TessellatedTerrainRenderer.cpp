#include "Core/stdafx.h"
#include "Terrain/TessellatedTerrainRenderer.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Core/Graphics.h"
#include "Graphics/Vertex.h"

#include <limits>

using namespace DirectX;

void TessellatedTerrainRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    BuildHeightTexture();
    BuildPatchGrid();
    m_dirty = false;
}

void TessellatedTerrainRenderer::OnDestroy()
{
    m_patches.Release();
    m_heightSRV.Reset();
    m_heightTexture.Reset();
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

    if (m_maxHeight <= m_minHeight)
    {
        m_minHeight = 0.0f;
        m_maxHeight = 1.0f;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = static_cast<UINT>(size);
    desc.Height           = static_cast<UINT>(size);
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = heights.data();
    data.SysMemPitch = static_cast<UINT>(sizeof(float) * size);

    if (DX_FAILED(m_graphics->GetDevice()->CreateTexture2D(&desc, &data,
                                                           m_heightTexture.ReleaseAndGetAddressOf()),
                  L"CreateTexture2D(heightmap)"))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format              = desc.Format;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (DX_FAILED(m_graphics->GetDevice()->CreateShaderResourceView(m_heightTexture.Get(), &srvDesc,
                                                                     m_heightSRV.ReleaseAndGetAddressOf()),
                  L"CreateShaderResourceView(heightmap)"))
        return false;

    dxutil::DebugLog(L"[Tess] 높이맵 텍스처 %d x %d (높이 %.1f ~ %.1f)",
                     size, size, m_minHeight, m_maxHeight);
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
