#include "Core/stdafx.h"
#include "Engine/SkyRenderer.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Graphics/Vertex.h"

using namespace DirectX;

void SkyRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;

    // 반지름은 far 평면보다 충분히 안쪽이어야 잘리지 않는다.
    BuildDome(32, 16, 400.0f);
}

void SkyRenderer::OnDestroy()
{
    m_dome.Release();
    m_graphics = nullptr;
}

void SkyRenderer::SetSunDirection(const XMFLOAT3& direction)
{
    XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&direction));
    XMStoreFloat3(&m_sunDirection, d);
}

// -------------------------------------------------------------
// 돔 메시 (S54)
//  위경도 방식으로 구를 만든다. 안쪽에서 보므로 삼각형 순서는
//  신경 쓰지 않는다(지형과 같은 CULL_NONE 상태를 쓴다).
//  아래쪽 절반은 지형에 가려지지만, 지형 밖을 내려다볼 때를 위해 남겨 둔다.
// -------------------------------------------------------------
bool SkyRenderer::BuildDome(int slices, int stacks, float radius)
{
    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(static_cast<size_t>(stacks + 1) * (slices + 1));

    for (int stack = 0; stack <= stacks; ++stack)
    {
        // phi : 위(0)에서 아래(pi)로
        const float phi = XM_PI * static_cast<float>(stack) / stacks;
        const float y = std::cos(phi);
        const float ringRadius = std::sin(phi);

        for (int slice = 0; slice <= slices; ++slice)
        {
            const float theta = XM_2PI * static_cast<float>(slice) / slices;

            TerrainVertex vertex{};
            vertex.position = XMFLOAT3(ringRadius * std::cos(theta) * radius,
                                       y * radius,
                                       ringRadius * std::sin(theta) * radius);

            // 픽셀 셰이더가 방향으로 색을 정하므로 법선에 방향을 담아 둔다.
            vertex.normal = XMFLOAT3(ringRadius * std::cos(theta), y, ringRadius * std::sin(theta));
            vertex.uv = XMFLOAT2(static_cast<float>(slice) / slices,
                                 static_cast<float>(stack) / stacks);
            vertex.morphTargets = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

            vertices.push_back(vertex);
        }
    }

    const int ring = slices + 1;
    for (int stack = 0; stack < stacks; ++stack)
    {
        for (int slice = 0; slice < slices; ++slice)
        {
            const uint32_t a = static_cast<uint32_t>(stack * ring + slice);
            const uint32_t b = a + 1;
            const uint32_t c = static_cast<uint32_t>((stack + 1) * ring + slice);
            const uint32_t d = c + 1;

            indices.push_back(a); indices.push_back(b); indices.push_back(c);
            indices.push_back(c); indices.push_back(b); indices.push_back(d);
        }
    }

    dxutil::DebugLog(L"[Sky] 돔 생성 : 정점 %zu개, 삼각형 %zu개",
                     vertices.size(), indices.size() / 3);

    return m_dome.Create(m_graphics->GetDevice(),
                         vertices.data(), static_cast<UINT>(vertices.size()),
                         static_cast<UINT>(sizeof(TerrainVertex)),
                         indices.data(), static_cast<UINT>(indices.size()));
}

void SkyRenderer::Update()
{
    // 구름이 흘러가려면 시간이 필요하다.
    m_time += TimeManager::Get().GetDeltaTime();
}

void SkyRenderer::Render()
{
    if (!m_graphics || !m_dome.IsValid())
        return;

    // 돔을 카메라 위치로 옮긴다. 회전은 주지 않는다.
    const XMFLOAT3 eye = m_graphics->GetEyePosition3D();
    const XMMATRIX world = XMMatrixTranslation(eye.x, eye.y, eye.z);

    Graphics::SkyDrawParams params;
    params.horizonColor = m_horizonColor;
    params.zenithColor = m_zenithColor;
    params.sunDirection = XMFLOAT4(m_sunDirection.x, m_sunDirection.y, m_sunDirection.z, 0.0f);
    params.params = XMFLOAT4(m_time,
                             m_cloudsEnabled ? 1.0f : 0.0f,
                             m_cloudCoverage,
                             m_cloudSpeed);

    m_graphics->DrawSky(m_dome, world, params);
}
