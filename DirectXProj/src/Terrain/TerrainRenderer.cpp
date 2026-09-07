#include "Core/stdafx.h"
#include "Terrain/TerrainRenderer.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Core/Graphics.h"

using namespace DirectX;

void TerrainRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    RebuildMesh();
}

void TerrainRenderer::OnDestroy()
{
    m_mesh.Release();
    m_graphics = nullptr;
}

void TerrainRenderer::SetGrid(int cellsX, int cellsZ, float cellSize)
{
    m_desc.cellsX = cellsX;
    m_desc.cellsZ = cellsZ;
    m_desc.cellSize = cellSize;
    m_dirty = true;
}

bool TerrainRenderer::RebuildMesh()
{
    m_dirty = false;

    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    terrain::MeshData data;
    if (!terrain::BuildGrid(m_desc, data))
        return false;

    return m_mesh.Create(m_graphics->GetDevice(),
                         data.vertices.data(),
                         static_cast<UINT>(data.vertices.size()),
                         static_cast<UINT>(sizeof(TerrainVertex)),
                         data.indices.data(),
                         static_cast<UINT>(data.indices.size()));
}

void TerrainRenderer::Update()
{
    // 설정이 바뀐 경우에만 다시 만든다. (매 프레임 재생성은 낭비다)
    if (m_dirty)
        RebuildMesh();
}

void TerrainRenderer::Render()
{
    if (!m_graphics || !m_mesh.IsValid())
        return;

    Transform* transform = GetTransform();
    if (!transform)
        return;

    // 격자선을 픽셀 셰이더에서 그리기 위해 칸 수를 넘긴다.
    const XMFLOAT4 params(static_cast<float>(m_desc.cellsX) * m_desc.uvTiling,
                          static_cast<float>(m_desc.cellsZ) * m_desc.uvTiling,
                          m_wireframe ? 1.0f : 0.0f,
                          0.0f);

    m_graphics->DrawMesh(m_mesh,
                         transform->GetWorldMatrix(),
                         m_wireframe ? m_wireColor : m_color,
                         params,
                         m_wireframe);
}

void TerrainRenderer::ToJson(json::Value& out) const
{
    Component::ToJson(out);

    out["cellsX"]    = json::Value(m_desc.cellsX);
    out["cellsZ"]    = json::Value(m_desc.cellsZ);
    out["cellSize"]  = json::Value(m_desc.cellSize);
    out["uvTiling"]  = json::Value(m_desc.uvTiling);
    out["wireframe"] = json::Value(m_wireframe);

    json::Value color = json::Value::MakeArray();
    color.Push(json::Value(m_color.x));
    color.Push(json::Value(m_color.y));
    color.Push(json::Value(m_color.z));
    color.Push(json::Value(m_color.w));
    out["color"] = std::move(color);
}

void TerrainRenderer::FromJson(const json::Value& in)
{
    Component::FromJson(in);

    if (const json::Value* value = in.Find("cellsX"))    m_desc.cellsX   = value->AsInt(m_desc.cellsX);
    if (const json::Value* value = in.Find("cellsZ"))    m_desc.cellsZ   = value->AsInt(m_desc.cellsZ);
    if (const json::Value* value = in.Find("cellSize"))  m_desc.cellSize = value->AsFloat(m_desc.cellSize);
    if (const json::Value* value = in.Find("uvTiling"))  m_desc.uvTiling = value->AsFloat(m_desc.uvTiling);
    if (const json::Value* value = in.Find("wireframe")) m_wireframe     = value->AsBool(m_wireframe);

    if (const json::Value* color = in.Find("color"))
    {
        m_color.x = color->At(0).AsFloat(m_color.x);
        m_color.y = color->At(1).AsFloat(m_color.y);
        m_color.z = color->At(2).AsFloat(m_color.z);
        m_color.w = color->At(3).AsFloat(m_color.w);
    }

    m_dirty = true;
}
