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

void TerrainRenderer::SetHeightEnabled(bool enabled)
{
    m_heightEnabled = enabled;
    m_dirty = true;
}

void TerrainRenderer::SetHeightParams(const terrain::HeightParams& params)
{
    m_height.SetParams(params);
    m_dirty = true;
}

void TerrainRenderer::Regenerate(unsigned seed)
{
    terrain::HeightParams params = m_height.GetParams();
    params.seed = seed;
    m_height.SetParams(params);
    m_dirty = true;
}

bool TerrainRenderer::RebuildMesh()
{
    m_dirty = false;

    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    terrain::MeshData data;
    if (!terrain::BuildGrid(m_desc, data, m_heightEnabled ? &m_height : nullptr))
        return false;

    m_heightRange = terrain::GetHeightRange(data);

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

    Graphics::MeshDrawParams draw;

    // 격자선을 픽셀 셰이더에서 그리기 위해 칸 수를 넘긴다.
    draw.params = XMFLOAT4(static_cast<float>(m_desc.cellsX) * m_desc.uvTiling,
                           static_cast<float>(m_desc.cellsZ) * m_desc.uvTiling,
                           m_wireframe ? 1.0f : 0.0f,
                           m_heightEnabled ? 1.0f : 0.0f);

    draw.color = m_wireframe ? m_wireColor : m_color;
    draw.heightRange = XMFLOAT4(m_heightRange.minY, m_heightRange.maxY, 0.0f, 0.0f);
    draw.wireframe = m_wireframe;

    m_graphics->DrawMesh(m_mesh, transform->GetWorldMatrix(), draw);
}

void TerrainRenderer::ToJson(json::Value& out) const
{
    Component::ToJson(out);

    out["cellsX"]    = json::Value(m_desc.cellsX);
    out["cellsZ"]    = json::Value(m_desc.cellsZ);
    out["cellSize"]  = json::Value(m_desc.cellSize);
    out["uvTiling"]  = json::Value(m_desc.uvTiling);
    out["wireframe"] = json::Value(m_wireframe);
    out["heightEnabled"] = json::Value(m_heightEnabled);

    const terrain::HeightParams& height = m_height.GetParams();
    out["seed"]        = json::Value(static_cast<uint64_t>(height.seed));
    out["frequency"]   = json::Value(height.frequency);
    out["amplitude"]   = json::Value(height.amplitude);
    out["octaves"]     = json::Value(height.octaves);
    out["persistence"] = json::Value(height.persistence);
    out["lacunarity"]  = json::Value(height.lacunarity);

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
    if (const json::Value* value = in.Find("wireframe"))     m_wireframe     = value->AsBool(m_wireframe);
    if (const json::Value* value = in.Find("heightEnabled")) m_heightEnabled = value->AsBool(m_heightEnabled);

    terrain::HeightParams height = m_height.GetParams();
    if (const json::Value* value = in.Find("seed"))        height.seed        = static_cast<unsigned>(value->AsUInt64(height.seed));
    if (const json::Value* value = in.Find("frequency"))   height.frequency   = value->AsFloat(height.frequency);
    if (const json::Value* value = in.Find("amplitude"))   height.amplitude   = value->AsFloat(height.amplitude);
    if (const json::Value* value = in.Find("octaves"))     height.octaves     = value->AsInt(height.octaves);
    if (const json::Value* value = in.Find("persistence")) height.persistence = value->AsFloat(height.persistence);
    if (const json::Value* value = in.Find("lacunarity"))  height.lacunarity  = value->AsFloat(height.lacunarity);
    m_height.SetParams(height);

    if (const json::Value* color = in.Find("color"))
    {
        m_color.x = color->At(0).AsFloat(m_color.x);
        m_color.y = color->At(1).AsFloat(m_color.y);
        m_color.z = color->At(2).AsFloat(m_color.z);
        m_color.w = color->At(3).AsFloat(m_color.w);
    }

    m_dirty = true;
}
