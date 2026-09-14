#include "Core/stdafx.h"
#include "Engine/WaterRenderer.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Graphics/Vertex.h"

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
}

void WaterRenderer::OnDestroy()
{
    m_plane.Release();
    m_graphics = nullptr;
}

void WaterRenderer::Update()
{
    m_time += TimeManager::Get().GetDeltaTime();
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

    m_graphics->DrawWater(m_plane, world, params);
}

void WaterRenderer::ToJson(json::Value& out) const
{
    Component::ToJson(out);
    out["level"]      = json::Value(m_level);
    out["reflection"] = json::Value(m_reflection);
    out["refraction"] = json::Value(m_refraction);
    out["foam"]       = json::Value(m_foam);
}

void WaterRenderer::FromJson(const json::Value& in)
{
    Component::FromJson(in);
    if (const json::Value* value = in.Find("level"))      m_level      = value->AsFloat(m_level);
    if (const json::Value* value = in.Find("reflection")) m_reflection = value->AsBool(m_reflection);
    if (const json::Value* value = in.Find("refraction")) m_refraction = value->AsBool(m_refraction);
    if (const json::Value* value = in.Find("foam"))       m_foam       = value->AsBool(m_foam);
}
