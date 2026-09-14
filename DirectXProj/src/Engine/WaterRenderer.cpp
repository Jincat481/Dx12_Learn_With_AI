#include "Core/stdafx.h"
#include "Engine/WaterRenderer.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Graphics/Vertex.h"
#include "Engine/GameObject.h"
#include "Engine/Scene.h"
#include "Engine/GroundRaycast.h"
#include "Input/InputManager.h"
#include "Engine/GroundProvider.h"

#include <cmath>

using namespace DirectX;

void WaterRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    if (!m_graphics || !m_graphics->GetDevice())
        return;

    // 정점 4개짜리 큰 판. 물결은 픽셀 셰이더가 법선으로 만든다.
    TerrainVertex vertices[4] = {};
    vertices[0].position = XMFLOAT3(-kHalfSize, 0.0f,  kHalfSize);
    vertices[1].position = XMFLOAT3( kHalfSize, 0.0f,  kHalfSize);
    vertices[2].position = XMFLOAT3(-kHalfSize, 0.0f, -kHalfSize);
    vertices[3].position = XMFLOAT3( kHalfSize, 0.0f, -kHalfSize);

    for (TerrainVertex& vertex : vertices)
        vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

    const uint32_t indices[6] = { 0, 1, 2, 2, 1, 3 };

    m_plane.Create(m_graphics->GetDevice(), vertices, 4, static_cast<UINT>(sizeof(TerrainVertex)), indices, 6);

    // 카메라 주변 384 x 384 영역을 512 x 512 텍셀로 시뮬레이션한다 (텍셀 하나 0.75).
    m_ripples.Initialize(m_graphics->GetDevice(), 512, 384.0f);

    // 해안 마스크 텍스처 (S81) : 1 바이트짜리 128 x 128. 처음엔 전부 물.
    D3D11_TEXTURE2D_DESC shore = {};
    shore.Width            = kShoreSize;
    shore.Height           = kShoreSize;
    shore.MipLevels        = 1;
    shore.ArraySize        = 1;
    shore.Format           = DXGI_FORMAT_R8_UNORM;
    shore.SampleDesc.Count = 1;
    shore.Usage            = D3D11_USAGE_DEFAULT;
    shore.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    std::vector<uint8_t> water(static_cast<size_t>(kShoreSize) * kShoreSize, 0);
    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = water.data();
    data.SysMemPitch = kShoreSize;

    if (FAILED(m_graphics->GetDevice()->CreateTexture2D(&shore, &data, m_shoreTexture.GetAddressOf())) ||
        FAILED(m_graphics->GetDevice()->CreateShaderResourceView(m_shoreTexture.Get(), nullptr, m_shoreView.GetAddressOf())))
    {
        m_shoreView.Reset();
        m_shoreTexture.Reset();
    }
}

void WaterRenderer::OnDestroy()
{
    m_ripples.Release();
    m_shoreView.Reset();
    m_shoreTexture.Reset();
    m_shoreProviders.clear();
    m_plane.Release();
    m_graphics = nullptr;
}

void WaterRenderer::Update()
{
    const float deltaTime = (std::min)(TimeManager::Get().GetDeltaTime(), 0.25f);
    m_time += deltaTime;

    if (!m_graphics || !m_ripples.IsValid())
        return;

    const InputManager& input = InputManager::Get();

    // ---- 클릭 / 드래그로 물방울 (S79) ----
    //  누른 순간은 크게, 누른 채 끄는 동안은 지나간 자리마다 작게 떨어뜨린다.
    //  우클릭은 카메라가, 패널 위 마우스는 UI 가 쓴다.
    const bool pointerFree = !input.IsPointerOverUI() && !input.GetMouseButton(InputManager::Right);

    if (pointerFree && input.GetMouseButton(InputManager::Left))
    {
        XMFLOAT3 hit{};
        if (PickWaterSurface(input.GetMouseX(), input.GetMouseY(), hit))
        {
            const bool pressed = input.GetMouseButtonDown(InputManager::Left);
            const float dx = hit.x - m_lastDrop.x;
            const float dz = hit.z - m_lastDrop.z;

            if (pressed || !m_dragging || dx * dx + dz * dz > 1.5f)
            {
                m_ripples.AddDrop(hit.x, hit.z, pressed ? 3.0f : 1.6f, pressed ? 3.0f : 0.9f);
                m_lastDrop = hit;
                m_dragging = true;
            }
        }
    }
    else
    {
        m_dragging = false;
    }

    // ---- 해안 마스크를 조금씩 굽는다 (S81) ----
    UpdateShoreMask();

    // ---- 고정 간격으로 시뮬레이션 진행 ----
    m_stepAccumulator += deltaTime;

    int steps = 0;
    while (m_stepAccumulator >= kStepSeconds && steps < kMaxStepsPerFrame)
    {
        if (m_rain)
            AddRainDrops();

        const XMFLOAT3 eye = m_graphics->GetEyePosition3D();
        m_ripples.Step(m_graphics->GetContext(), eye.x, eye.z);

        m_stepAccumulator -= kStepSeconds;
        ++steps;
    }

    // 너무 밀렸으면(씬 로딩 직후 등) 밀린 시간은 버린다. 한꺼번에 따라잡으면 그 프레임이 멈춘다.
    if (steps == kMaxStepsPerFrame)
        m_stepAccumulator = 0.0f;
}

// -------------------------------------------------------------
// 마우스 레이와 수면의 교점
//  수면은 y = 수위인 평면이라 식 하나로 풀린다 : origin.y + t · dir.y = 수위
//  다만 그 앞에서 섬이나 산에 먼저 닿았다면 물을 클릭한 것이 아니다.
// -------------------------------------------------------------
bool WaterRenderer::PickWaterSurface(int mouseX, int mouseY, XMFLOAT3& outHit) const
{
    XMFLOAT3 origin{};
    XMFLOAT3 direction{};
    if (!m_graphics->ScreenToRay(mouseX, mouseY, origin, direction))
        return false;

    if (direction.y > -1.0e-4f)
        return false;   // 수평이나 위를 보면 수면에 닿지 않는다

    const float t = (m_level - origin.y) / direction.y;
    if (t <= 0.0f)
        return false;   // 카메라가 물속에 있다

    const GameObject* owner = GetOwner();
    const Scene* scene = owner ? owner->GetScene() : nullptr;

    XMFLOAT3 groundHit{};
    if (scene && ground::RaycastScene(*scene, origin, direction, t, groundHit))
        return false;   // 수면보다 앞에서 땅에 막혔다

    outHit = XMFLOAT3(origin.x + direction.x * t, m_level, origin.z + direction.z * t);
    return true;
}

// -------------------------------------------------------------
// 해안 마스크 (S81)
//  물결 영역 위의 128 x 128 점마다 땅 높이를 물어, 수면보다 높으면 "땅(1)" 으로 적는다.
//  시뮬레이션은 땅인 텍셀의 높이를 0 에 묶어 벽으로 쓴다 → 물결이 섬과 해안에서 반사된다.
//
//  지면 질의는 노이즈 계산이라 1만 6천 번을 한 프레임에 하면 멈춘다.
//  한 프레임에 몇 줄씩만 굽고, 다 굽은 뒤에 한 번에 GPU 로 올린다(그동안은 이전 마스크를 쓴다).
//  영역이 크게 옮겨졌거나 수위가 바뀌었을 때만 새로 굽는다.
// -------------------------------------------------------------
void WaterRenderer::UpdateShoreMask()
{
    if (!m_shoreTexture || !m_graphics)
        return;

    if (m_shoreRow < 0)
    {
        const XMFLOAT3 region = m_ripples.GetRegion();
        const float moved = (std::max)(std::fabs(region.x - m_shoreRegion.x), std::fabs(region.y - m_shoreRegion.y));
        const bool levelChanged = std::fabs(m_level - m_shoreLevel) > 0.01f;

        if (m_hasShore && moved < region.z * 0.1f && !levelChanged)
            return;

        // 새로 굽기 시작 : 이번에 물어볼 지형 컴포넌트를 모아 둔다.
        m_shoreProviders.clear();
        const GameObject* owner = GetOwner();
        if (const Scene* scene = owner ? owner->GetScene() : nullptr)
        {
            for (const auto& object : scene->GetGameObjects())
            {
                if (!object || object->IsPendingDestroy() || !object->IsActive())
                    continue;

                for (const auto& entry : object->GetComponentMap())
                {
                    for (const auto& component : entry.second)
                    {
                        if (const IGroundProvider* provider = dynamic_cast<const IGroundProvider*>(component.get()))
                            m_shoreProviders.push_back(provider);
                    }
                }
            }
        }

        if (m_shoreProviders.empty())
            return;

        m_shoreBuildRegion = region;
        m_shoreBuildLevel = m_level;
        m_shoreBuilding.assign(static_cast<size_t>(kShoreSize) * kShoreSize, 0);
        m_shoreRow = 0;
    }

    const float cell = m_shoreBuildRegion.z / kShoreSize;
    const int lastRow = (std::min)(kShoreSize, m_shoreRow + kShoreRowsPerFrame);

    for (int row = m_shoreRow; row < lastRow; ++row)
    {
        // 마스크의 행은 +Z 로 커진다 (물결 텍스처와 같은 규약)
        const float z = m_shoreBuildRegion.y + (row + 0.5f) * cell;

        for (int column = 0; column < kShoreSize; ++column)
        {
            const float x = m_shoreBuildRegion.x + (column + 0.5f) * cell;

            bool land = false;
            for (const IGroundProvider* provider : m_shoreProviders)
            {
                float height = 0.0f;
                // 수면 위로 드러난 땅만 벽이다. 얕은 물까지 벽으로 두면 물가에 떨어뜨린 물방울이 바로 지워진다.
                if (provider->TryGetGroundHeight(x, z, height) && height > m_shoreBuildLevel)
                {
                    land = true;
                    break;
                }
            }

            m_shoreBuilding[static_cast<size_t>(row) * kShoreSize + column] = land ? 255 : 0;
        }
    }

    m_shoreRow = lastRow;

    if (m_shoreRow >= kShoreSize)
    {
        m_graphics->GetContext()->UpdateSubresource(m_shoreTexture.Get(), 0, nullptr,
                                                    m_shoreBuilding.data(), kShoreSize, 0);
        m_shoreRegion = m_shoreBuildRegion;
        m_shoreLevel = m_shoreBuildLevel;
        m_hasShore = true;
        m_shoreRow = -1;

        m_ripples.SetShoreMask(m_shoreView.Get(), m_shoreRegion);
    }
}

// 빗방울 : 카메라 주변에 작고 약한 물방울을 무작위로 떨어뜨린다.
void WaterRenderer::AddRainDrops()
{
    const XMFLOAT3 eye = m_graphics->GetEyePosition3D();

    std::uniform_real_distribution<float> offset(-150.0f, 150.0f);
    std::uniform_real_distribution<float> radius(0.6f, 1.1f);
    std::uniform_real_distribution<float> strength(0.2f, 0.5f);

    for (int i = 0; i < 2; ++i)
        m_ripples.AddDrop(eye.x + offset(m_rng), eye.z + offset(m_rng), radius(m_rng), strength(m_rng));
}

void WaterRenderer::RenderTransparent()
{
    if (!m_graphics || !m_plane.IsValid())
        return;

    // 반사 패스에서는 물 자신을 그리지 않는다. (자기 자신을 비출 수는 없다)
    if (m_graphics->GetRenderPass() != Graphics::RenderPass::Main)
        return;

    // 판을 카메라 발밑으로 옮긴다. 물결은 월드 좌표로 계산하므로 판이 움직여도 흐름이 튀지 않는다.
    const XMFLOAT3 eye = m_graphics->GetEyePosition3D();
    const XMMATRIX world = XMMatrixTranslation(eye.x, m_level, eye.z);

    Graphics::WaterDrawParams params;
    params.time = m_time;
    params.reflection = m_reflection;
    params.refraction = m_refraction;
    params.foam = m_foam;
    params.flow = m_flow;
    params.rippleDebug = m_rippleDebug;

    if (m_hasShore && m_shoreView)
    {
        params.shoreMask = m_shoreView.Get();
        params.shoreRegion = XMFLOAT4(m_shoreRegion.x, m_shoreRegion.y, m_shoreRegion.z, 1.0f);
    }

    if (m_ripples.IsValid())
    {
        const XMFLOAT3 region = m_ripples.GetRegion();
        params.rippleHeight = m_ripples.GetHeightSRV();
        params.rippleRegion = XMFLOAT4(region.x, region.y, region.z, 1.0f);
        params.rippleTexel = 1.0f / static_cast<float>(m_ripples.GetSize());
    }

    m_graphics->DrawWater(m_plane, world, params);
}

void WaterRenderer::ToJson(json::Value& out) const
{
    Component::ToJson(out);
    out["level"]      = json::Value(m_level);
    out["reflection"] = json::Value(m_reflection);
    out["refraction"] = json::Value(m_refraction);
    out["foam"]       = json::Value(m_foam);
    out["flow"]       = json::Value(m_flow);
    out["rain"]       = json::Value(m_rain);
}

void WaterRenderer::FromJson(const json::Value& in)
{
    Component::FromJson(in);
    if (const json::Value* value = in.Find("level"))      m_level      = value->AsFloat(m_level);
    if (const json::Value* value = in.Find("reflection")) m_reflection = value->AsBool(m_reflection);
    if (const json::Value* value = in.Find("refraction")) m_refraction = value->AsBool(m_refraction);
    if (const json::Value* value = in.Find("foam"))       m_foam       = value->AsBool(m_foam);
    if (const json::Value* value = in.Find("flow"))       m_flow       = value->AsBool(m_flow);
    if (const json::Value* value = in.Find("rain"))       m_rain       = value->AsBool(m_rain);
}
