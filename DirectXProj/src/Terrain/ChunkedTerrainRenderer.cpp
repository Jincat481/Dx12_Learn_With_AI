#include "Core/stdafx.h"
#include "Terrain/ChunkedTerrainRenderer.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Core/Graphics.h"
#include "Graphics/TextureManager.h"
#include "Utils/Paths.h"

using namespace DirectX;

void ChunkedTerrainRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    LoadSplatLayers();
    RebuildChunks();
}

void ChunkedTerrainRenderer::OnDestroy()
{
    for (terrain::TerrainChunk& chunk : m_chunks)
        chunk.Release();

    m_chunks.clear();
    m_quadTree.Clear();
    m_graphics = nullptr;
}

void ChunkedTerrainRenderer::LoadSplatLayers()
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

void ChunkedTerrainRenderer::SetGrid(int chunksX, int chunksZ, int cellsPerChunk, float cellSize)
{
    m_chunksX = chunksX;
    m_chunksZ = chunksZ;
    m_cellsPerChunk = cellsPerChunk;
    m_cellSize = cellSize;
    m_dirty = true;
}

void ChunkedTerrainRenderer::SetHeightParams(const terrain::HeightParams& params)
{
    m_height.SetParams(params);
    m_dirty = true;
}

void ChunkedTerrainRenderer::Regenerate(unsigned seed)
{
    terrain::HeightParams params = m_height.GetParams();
    params.seed = seed;
    m_height.SetParams(params);
    m_dirty = true;
}

// -------------------------------------------------------------
// 청크 생성
//  지형 전체가 원점을 중심으로 놓이도록 각 청크의 시작 좌표를 계산한다.
// -------------------------------------------------------------
bool ChunkedTerrainRenderer::RebuildChunks()
{
    m_dirty = false;

    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    for (terrain::TerrainChunk& chunk : m_chunks)
        chunk.Release();

    m_chunks.clear();
    m_chunks.resize(static_cast<size_t>(m_chunksX) * m_chunksZ);

    const float chunkSize = m_cellsPerChunk * m_cellSize;
    const float halfWidth = m_chunksX * chunkSize * 0.5f;
    const float halfDepth = m_chunksZ * chunkSize * 0.5f;

    for (int cz = 0; cz < m_chunksZ; ++cz)
    {
        for (int cx = 0; cx < m_chunksX; ++cx)
        {
            const float originX = -halfWidth + cx * chunkSize;
            const float originZ = halfDepth - cz * chunkSize;   // +Z 에서 -Z 로 내려간다

            const size_t index = static_cast<size_t>(cz) * m_chunksX + cx;
            m_chunks[index].Build(m_graphics->GetDevice(),
                                  originX, originZ,
                                  m_cellsPerChunk, m_cellSize,
                                  &m_height);
        }
    }

    m_quadTree.Build(m_chunksX, m_chunksZ, m_chunks);
    m_chunkLod.assign(m_chunks.size(), 0);

    dxutil::DebugLog(L"[ChunkedTerrain] 청크 %d x %d (칸 %d, 총 %zu개)",
                     m_chunksX, m_chunksZ, m_cellsPerChunk, m_chunks.size());
    return true;
}

// -------------------------------------------------------------
// LOD 선택 (S51)
//  카메라에서 멀수록 단계를 올린다. 화면에서 차지하는 크기가 작아지므로
//  삼각형을 줄여도 티가 나지 않는다.
// -------------------------------------------------------------
int ChunkedTerrainRenderer::SelectLod(const XMFLOAT3& chunkCenter, const XMFLOAT3& eye) const
{
    if (!m_lodEnabled)
        return 0;

    const float dx = chunkCenter.x - eye.x;
    const float dy = chunkCenter.y - eye.y;
    const float dz = chunkCenter.z - eye.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    const int lod = static_cast<int>(distance / m_lodDistance);
    return (std::max)(0, (std::min)(terrain::TerrainChunk::kMaxLod - 1, lod));
}

// -------------------------------------------------------------
// 매 프레임 : 절두체를 만들고 보이는 청크를 고른다.
// -------------------------------------------------------------
void ChunkedTerrainRenderer::Update()
{
    if (m_dirty)
        RebuildChunks();

    if (!m_graphics || m_chunks.empty())
        return;

    const XMFLOAT3 eye = m_graphics->GetEyePosition3D();

    if (m_cullingEnabled && m_quadTree.IsValid())
    {
        Frustum frustum;
        frustum.BuildFromViewProjection(m_graphics->GetView3D() * m_graphics->GetProjection3D());

        m_quadTree.Cull(frustum, m_visibleChunks, m_stats);
    }
    else
    {
        // 컬링을 끄면 전부 그린다. 비교용이다.
        m_visibleChunks.clear();
        m_visibleChunks.reserve(m_chunks.size());
        for (int i = 0; i < static_cast<int>(m_chunks.size()); ++i)
            m_visibleChunks.push_back(i);

        m_stats = terrain::TerrainQuadTree::Stats{};
        m_stats.totalChunks = static_cast<int>(m_chunks.size());
        m_stats.visibleChunks = static_cast<int>(m_visibleChunks.size());
    }

    // LOD 결정
    if (m_chunkLod.size() != m_chunks.size())
        m_chunkLod.assign(m_chunks.size(), 0);

    for (int index : m_visibleChunks)
    {
        if (index >= 0 && index < static_cast<int>(m_chunks.size()))
            m_chunkLod[index] = SelectLod(m_chunks[index].GetCenter(), eye);
    }
}

// -------------------------------------------------------------
// 디버그 색
// -------------------------------------------------------------
XMFLOAT4 ChunkedTerrainRenderer::GetDebugColor(int chunkIndex, int lod) const
{
    if (m_displayMode == DisplayMode::LodColor)
    {
        static const XMFLOAT4 kLodColors[terrain::TerrainChunk::kMaxLod] =
        {
            XMFLOAT4(0.35f, 0.85f, 0.40f, 1.0f),   // LOD 0 : 초록 (가장 촘촘)
            XMFLOAT4(0.95f, 0.85f, 0.30f, 1.0f),   // LOD 1 : 노랑
            XMFLOAT4(0.95f, 0.55f, 0.25f, 1.0f),   // LOD 2 : 주황
            XMFLOAT4(0.90f, 0.30f, 0.35f, 1.0f),   // LOD 3 : 빨강 (가장 성김)
        };
        return kLodColors[(std::max)(0, (std::min)(terrain::TerrainChunk::kMaxLod - 1, lod))];
    }

    // 청크마다 다른 색. 인접한 청크가 같은 색이 되지 않도록 섞는다.
    const unsigned h = static_cast<unsigned>(chunkIndex) * 2654435761u;
    const float r = 0.35f + ((h >> 16) & 0xFF) / 255.0f * 0.55f;
    const float g = 0.35f + ((h >> 8) & 0xFF) / 255.0f * 0.55f;
    const float b = 0.35f + (h & 0xFF) / 255.0f * 0.55f;
    return XMFLOAT4(r, g, b, 1.0f);
}

void ChunkedTerrainRenderer::Render()
{
    m_drawnTriangles = 0;

    if (!m_graphics || m_chunks.empty())
        return;

    Transform* transform = GetTransform();
    if (!transform)
        return;

    const XMMATRIX world = transform->GetWorldMatrix();

    const bool wireframe = (m_displayMode == DisplayMode::Wireframe);
    const bool splat = (m_displayMode == DisplayMode::Splatting);
    const bool debugColor = (m_displayMode == DisplayMode::ChunkColor ||
                             m_displayMode == DisplayMode::LodColor);

    for (int index : m_visibleChunks)
    {
        if (index < 0 || index >= static_cast<int>(m_chunks.size()))
            continue;

        const terrain::TerrainChunk& chunk = m_chunks[index];
        if (!chunk.IsValid())
            continue;

        const int lod = m_chunkLod[index];

        Graphics::MeshDrawParams draw;
        draw.indexOffset = chunk.GetIndexOffset(lod);
        draw.indexCount = chunk.GetIndexCount(lod);
        draw.wireframe = wireframe;

        // 청크 UV 는 0~1 이므로 격자선 칸 수는 청크 기준이다.
        draw.params = XMFLOAT4(static_cast<float>(m_cellsPerChunk),
                               static_cast<float>(m_cellsPerChunk),
                               wireframe ? 1.0f : 0.0f,
                               1.0f);

        draw.heightRange = XMFLOAT4(chunk.GetMinHeight(), chunk.GetMaxHeight(), 0.0f, 0.0f);
        draw.splat = XMFLOAT4(6.0f, splat ? 1.0f : 0.0f, debugColor ? 1.0f : 0.0f, 0.0f);

        if (wireframe)
            draw.color = m_wireColor;
        else if (debugColor)
            draw.color = GetDebugColor(index, lod);

        if (splat)
        {
            for (int i = 0; i < kLayerCount; ++i)
                draw.layers[i] = m_layers[i] ? m_layers[i]->GetSRV() : nullptr;
        }

        m_graphics->DrawMesh(chunk.GetMesh(), world, draw);
        m_drawnTriangles += static_cast<int>(draw.indexCount / 3);
    }
}

// -------------------------------------------------------------
// 표시 모드
// -------------------------------------------------------------
void ChunkedTerrainRenderer::SetDisplayMode(DisplayMode mode)
{
    m_displayMode = mode;
}

void ChunkedTerrainRenderer::CycleDisplayMode()
{
    const int next = (static_cast<int>(m_displayMode) + 1) % static_cast<int>(DisplayMode::Count);
    m_displayMode = static_cast<DisplayMode>(next);
}

const wchar_t* ChunkedTerrainRenderer::GetDisplayModeName() const
{
    switch (m_displayMode)
    {
    case DisplayMode::Splatting:   return L"텍스처 스플래팅";
    case DisplayMode::HeightColor: return L"높이 색상";
    case DisplayMode::ChunkColor:  return L"청크 색상";
    case DisplayMode::LodColor:    return L"LOD 색상";
    case DisplayMode::Wireframe:   return L"와이어프레임";
    default:                       return L"-";
    }
}
