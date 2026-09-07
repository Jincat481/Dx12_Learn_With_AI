#include "Core/stdafx.h"
#include "Terrain/TerrainRenderer.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Core/Graphics.h"
#include "Utils/Paths.h"
#include "Utils/StringUtil.h"
#include "Graphics/TextureManager.h"

using namespace DirectX;

void TerrainRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;

    if (m_splatEnabled)
        LoadSplatLayers();

    RebuildMesh();
}

// 레이어는 TextureManager 캐시를 거치므로 여러 터레인이 같은 장을 공유한다.
void TerrainRenderer::LoadSplatLayers()
{
    static const wchar_t* kLayerPaths[kLayerCount] =
    {
        L"Assets/terrain_dirt.png",
        L"Assets/terrain_grass.png",
        L"Assets/terrain_rock.png",
        L"Assets/terrain_snow.png",
    };

    for (int i = 0; i < kLayerCount; ++i)
        m_layers[i] = TextureManager::Get().Load(Paths::Resolve(kLayerPaths[i]));
}

// -------------------------------------------------------------
// 표시 모드
//  네 가지 보기 방식이 서로 배타적이므로 플래그를 한 번에 정한다.
// -------------------------------------------------------------
void TerrainRenderer::SetDisplayMode(DisplayMode mode)
{
    m_displayMode = mode;
    ApplyDisplayMode();
}

void TerrainRenderer::CycleDisplayMode()
{
    const int next = (static_cast<int>(m_displayMode) + 1) % static_cast<int>(DisplayMode::Count);
    SetDisplayMode(static_cast<DisplayMode>(next));
}

const wchar_t* TerrainRenderer::GetDisplayModeName() const
{
    switch (m_displayMode)
    {
    case DisplayMode::Splatting:   return L"텍스처 스플래팅";
    case DisplayMode::HeightColor: return L"높이 색상";
    case DisplayMode::Wireframe:   return L"와이어프레임";
    case DisplayMode::FlatGrid:    return L"평면 그리드";
    default:                       return L"-";
    }
}

void TerrainRenderer::ApplyDisplayMode()
{
    const bool wasHeightEnabled = m_heightEnabled;

    switch (m_displayMode)
    {
    case DisplayMode::Splatting:
        m_wireframe = false;
        m_heightEnabled = true;
        SetSplattingEnabled(true);
        break;

    case DisplayMode::HeightColor:
        m_wireframe = false;
        m_heightEnabled = true;
        m_splatEnabled = false;
        break;

    case DisplayMode::Wireframe:
        m_wireframe = true;
        m_heightEnabled = true;
        break;

    case DisplayMode::FlatGrid:
        m_wireframe = false;
        m_heightEnabled = false;
        m_splatEnabled = false;
        break;

    default:
        break;
    }

    // 높이 사용 여부가 바뀔 때만 메시를 다시 만든다.
    if (wasHeightEnabled != m_heightEnabled)
        m_dirty = true;
}

void TerrainRenderer::SetSplattingEnabled(bool enabled)
{
    m_splatEnabled = enabled;

    if (enabled && m_graphics)
        LoadSplatLayers();
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

void TerrainRenderer::SetHeightSourceNoise(float amplitude, float frequency)
{
    terrain::HeightParams params = m_height.GetParams();
    params.source = terrain::HeightSource::Noise;
    params.amplitude = amplitude;
    params.frequency = frequency;
    m_height.SetParams(params);
    m_dirty = true;
}

void TerrainRenderer::SetHeightSourceImage(const std::wstring& path, float amplitude)
{
    auto image = std::make_shared<terrain::HeightMapImage>();
    if (!image->LoadFromFile(Paths::Resolve(path)))
    {
        dxutil::DebugLog(L"[Terrain] 높이맵 로드 실패, 노이즈로 되돌린다 : %s", path.c_str());
        SetHeightSourceNoise();
        return;
    }

    terrain::HeightParams params = m_height.GetParams();
    params.source = terrain::HeightSource::Image;
    params.amplitude = amplitude;
    m_height.SetParams(params);
    m_height.SetImage(std::move(image));

    m_imagePath = path;
    m_dirty = true;
}

void TerrainRenderer::SetNoiseType(terrain::NoiseType type)
{
    terrain::HeightParams params = m_height.GetParams();
    params.noiseType = type;
    m_height.SetParams(params);
    m_dirty = true;
}

void TerrainRenderer::ToggleNoiseType()
{
    SetNoiseType(GetNoiseType() == terrain::NoiseType::Perlin
                 ? terrain::NoiseType::Value
                 : terrain::NoiseType::Perlin);
}

bool TerrainRenderer::RebuildMesh()
{
    m_dirty = false;

    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    // 이미지 모드는 월드 크기를 알아야 UV 를 만들 수 있다.
    terrain::HeightParams params = m_height.GetParams();
    params.worldWidth = m_desc.GetWidth();
    params.worldDepth = m_desc.GetDepth();
    m_height.SetParams(params);

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

    // 스플래팅 : 와이어프레임일 때는 의미가 없으므로 끈다.
    const bool splat = m_splatEnabled && !m_wireframe;
    draw.splat = XMFLOAT4(m_splatTiling, splat ? 1.0f : 0.0f, 0.0f, 0.0f);

    if (splat)
    {
        for (int i = 0; i < kLayerCount; ++i)
            draw.layers[i] = m_layers[i] ? m_layers[i]->GetSRV() : nullptr;
    }

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
    out["splatting"]  = json::Value(m_splatEnabled);
    out["splatTiling"] = json::Value(m_splatTiling);
    out["heightEnabled"] = json::Value(m_heightEnabled);

    const terrain::HeightParams& height = m_height.GetParams();
    out["source"]      = json::Value(std::string(height.source == terrain::HeightSource::Image ? "image" : "noise"));
    out["heightMap"]   = json::Value(StringUtil::WideToUtf8(m_imagePath));
    out["noiseType"]   = json::Value(std::string(height.noiseType == terrain::NoiseType::Perlin ? "perlin" : "value"));
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
    if (const json::Value* value = in.Find("splatting"))     m_splatEnabled  = value->AsBool(m_splatEnabled);
    if (const json::Value* value = in.Find("splatTiling"))   m_splatTiling   = value->AsFloat(m_splatTiling);
    if (const json::Value* value = in.Find("heightEnabled")) m_heightEnabled = value->AsBool(m_heightEnabled);

    terrain::HeightParams height = m_height.GetParams();
    if (const json::Value* value = in.Find("noiseType"))
        height.noiseType = (value->AsString("perlin") == "value") ? terrain::NoiseType::Value : terrain::NoiseType::Perlin;
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
