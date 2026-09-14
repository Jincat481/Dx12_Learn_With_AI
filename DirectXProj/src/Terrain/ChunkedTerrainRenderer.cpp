#include "Core/stdafx.h"
#include "Terrain/ChunkedTerrainRenderer.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Core/Graphics.h"
#include "Graphics/TextureManager.h"
#include "Utils/Paths.h"
#include "Core/TimeManager.h"

#include <limits>
#include <algorithm>
#include <chrono>
#include <cmath>

using namespace DirectX;

void ChunkedTerrainRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    LoadSplatLayers();

    // 작업 스레드 (S65). 코어 하나는 메인 스레드(입력·렌더링)에 남겨 둔다.
    const int cores = static_cast<int>(std::thread::hardware_concurrency());
    m_worker.Start((std::max)(1, (std::min)(4, cores - 1)));

    RebuildChunks();
}

void ChunkedTerrainRenderer::OnDestroy()
{
    // 청크를 해제하기 전에 스레드부터 멈춘다. (작업 스레드는 청크를 만지지 않지만 순서를 지켜 둔다)
    m_worker.Stop();
    m_inFlight.clear();

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

    // 지형 설정이 바뀌었다. 옛 설정으로 주문한 작업은 모두 무효다. (S65)
    //  세대 번호를 올려 두면 늦게 도착한 옛 결과를 알아보고 버릴 수 있다.
    ++m_generation;
    m_worker.CancelPending();
    m_inFlight.clear();
    m_heightSnapshot = std::make_shared<terrain::HeightField>(m_height);

    for (terrain::TerrainChunk& chunk : m_chunks)
        chunk.Release();

    m_chunks.clear();
    m_chunks.resize(static_cast<size_t>(m_chunksX) * m_chunksZ);

    m_slots.assign(m_chunks.size(), ChunkSlot{});

    // 슬롯 (cx, cz) 가 담당할 월드 청크 좌표는 중심에서의 상대 위치로 정한다.
    //  유한 지형이면 중심이 0 이라 원점 대칭으로 놓이고,
    //  무한 지형이면 중심이 카메라를 따라 움직인다.
    const int halfX = m_chunksX / 2;
    const int halfZ = m_chunksZ / 2;

    for (int cz = 0; cz < m_chunksZ; ++cz)
    {
        for (int cx = 0; cx < m_chunksX; ++cx)
        {
            const size_t index = static_cast<size_t>(cz) * m_chunksX + cx;
            BuildChunkAt(index,
                         m_centerChunkX + (cx - halfX),
                         m_centerChunkZ + (halfZ - cz));   // cz 가 커질수록 -Z
        }
    }

    // 전체 높이 범위를 모아 둔다. (색상 램프 / 스플래팅 기준)
    m_globalMinHeight = (std::numeric_limits<float>::max)();
    m_globalMaxHeight = -(std::numeric_limits<float>::max)();

    for (const terrain::TerrainChunk& chunk : m_chunks)
    {
        if (!chunk.IsValid())
            continue;

        m_globalMinHeight = (std::min)(m_globalMinHeight, chunk.GetMinHeight());
        m_globalMaxHeight = (std::max)(m_globalMaxHeight, chunk.GetMaxHeight());
    }

    if (m_globalMaxHeight <= m_globalMinHeight)
    {
        m_globalMinHeight = 0.0f;
        m_globalMaxHeight = 1.0f;
    }

    m_quadTree.Build(m_chunksX, m_chunksZ, m_chunks);
    m_chunkLod.assign(m_chunks.size(), 0);
    m_chunkMorph.assign(m_chunks.size(), 0.0f);

    dxutil::DebugLog(L"[ChunkedTerrain] 청크 %d x %d (칸 %d, 총 %zu개)",
                     m_chunksX, m_chunksZ, m_cellsPerChunk, m_chunks.size());
    return true;
}

// 슬롯 하나를 지정한 월드 청크 좌표로 다시 만든다.
bool ChunkedTerrainRenderer::BuildChunkAt(size_t slot, int worldChunkX, int worldChunkZ)
{
    if (slot >= m_chunks.size() || !m_graphics || !m_graphics->GetDevice())
        return false;

    const float chunkSize = m_cellsPerChunk * m_cellSize;

    // 청크 (cx, cz) 는 X 로 [cx, cx+1), Z 로 [cz, cz+1) 구간을 담당한다.
    // TerrainChunk 는 originZ 에서 -Z 방향으로 내려가므로 위쪽 모서리를 넘긴다.
    const float originX = worldChunkX * chunkSize;
    const float originZ = (worldChunkZ + 1) * chunkSize;

    m_chunks[slot].SetSkirtEnabled(m_skirtEnabled);
    const bool ok = m_chunks[slot].Build(m_graphics->GetDevice(),
                                         originX, originZ,
                                         m_cellsPerChunk, m_cellSize,
                                         &m_height);

    m_slots[slot] = ChunkSlot{ worldChunkX, worldChunkZ, ok };
    return ok;
}

void ChunkedTerrainRenderer::SetInfiniteEnabled(bool enabled)
{
    if (m_infiniteEnabled == enabled)
        return;

    m_infiniteEnabled = enabled;

    // 유한 지형으로 돌아갈 때는 원점 중심으로 되돌린다.
    if (!enabled)
    {
        m_centerChunkX = 0;
        m_centerChunkZ = 0;
        m_dirty = true;
    }
}

// -------------------------------------------------------------
// 무한 지형 (S58)
//  청크 배열의 크기는 그대로 두고, 카메라가 한 칸 움직일 때마다
//  뒤로 밀려난 슬롯을 앞쪽 좌표로 다시 만든다(재활용).
//  높이가 함수(노이즈)라서 좌표만 주면 어디든 만들 수 있기에 가능하다.
//  이미지 하이트맵은 범위가 정해져 있어 이 방식이 통하지 않는다.
// -------------------------------------------------------------
void ChunkedTerrainRenderer::UpdateInfiniteChunks(const XMFLOAT3& eye)
{
    m_rebuiltThisFrame = 0;

    const float chunkSize = m_cellsPerChunk * m_cellSize;
    if (chunkSize <= 0.0f)
        return;

    // 이 함수가 메인 스레드에서 쓴 시간을 잰다. 동기/비동기 차이를 숫자로 보여 주기 위해서다.
    const auto costStart = std::chrono::steady_clock::now();

    m_centerChunkX = static_cast<int>(std::floor(eye.x / chunkSize));
    m_centerChunkZ = static_cast<int>(std::floor(eye.z / chunkSize));

    const bool centerMoved = !m_hasLastCenter ||
                             m_centerChunkX != m_lastCenterX ||
                             m_centerChunkZ != m_lastCenterZ;
    m_lastCenterX = m_centerChunkX;
    m_lastCenterZ = m_centerChunkZ;
    m_hasLastCenter = true;

    const bool async = m_asyncBuild && m_worker.IsRunning();

    if (async)
    {
        // 1) 작업 스레드가 다 만든 청크를 GPU 에 올린다.
        ReceiveBuiltChunks();

        // 2) 카메라가 청크 경계를 넘었으면 줄 서 있는 요청의 "가까운 순서" 가 틀어졌다.
        //    아직 시작하지 않은 것은 거둬들이고, 아래에서 새 순서로 다시 줄 세운다.
        if (centerMoved)
        {
            for (const terrain::ChunkBuildRequest& canceled : m_worker.CancelPending())
                m_inFlight.erase(MakeChunkKey(canceled.worldX, canceled.worldZ));
        }
    }

    const int halfX = m_chunksX / 2;
    const int halfZ = m_chunksZ / 2;

    // 3) 지금 위치에 필요한데 아직 없는 청크를 모은다. 카메라에 가까운 것부터.
    struct Pending { size_t slot; int worldX; int worldZ; int distance; };
    std::vector<Pending> pending;

    for (int cz = 0; cz < m_chunksZ; ++cz)
    {
        for (int cx = 0; cx < m_chunksX; ++cx)
        {
            const size_t index = static_cast<size_t>(cz) * m_chunksX + cx;

            const int wantX = m_centerChunkX + (cx - halfX);
            const int wantZ = m_centerChunkZ + (halfZ - cz);

            const ChunkSlot& slot = m_slots[index];
            if (slot.valid && slot.worldX == wantX && slot.worldZ == wantZ)
                continue;

            const int dx = cx - halfX;
            const int dz = cz - halfZ;
            pending.push_back({ index, wantX, wantZ, dx * dx + dz * dz });
        }
    }

    if (!pending.empty())
    {
        std::sort(pending.begin(), pending.end(),
                  [](const Pending& a, const Pending& b) { return a.distance < b.distance; });

        if (async)
        {
            // 주문만 넣고 바로 돌아온다. 이미 주문한 좌표는 다시 넣지 않는다.
            for (const Pending& item : pending)
            {
                const uint64_t key = MakeChunkKey(item.worldX, item.worldZ);
                if (m_inFlight.count(key) != 0)
                    continue;

                terrain::ChunkBuildRequest request;
                request.worldX = item.worldX;
                request.worldZ = item.worldZ;
                request.generation = m_generation;
                request.originX = item.worldX * chunkSize;           // BuildChunkAt 과 같은 규약
                request.originZ = (item.worldZ + 1) * chunkSize;
                request.cells = m_cellsPerChunk;
                request.cellSize = m_cellSize;
                request.skirt = m_skirtEnabled;
                request.height = m_heightSnapshot;

                m_worker.Submit(std::move(request));
                m_inFlight.insert(key);
            }
        }
        else
        {
            // 동기 방식 : 메인 스레드가 직접 만든다. 만드는 시간만큼 이번 프레임이 늦어진다.
            const int budget = (std::min)(m_rebuildBudget, static_cast<int>(pending.size()));
            for (int i = 0; i < budget; ++i)
            {
                BuildChunkAt(pending[i].slot, pending[i].worldX, pending[i].worldZ);
                ++m_rebuiltThisFrame;
            }
        }
    }

    const auto costEnd = std::chrono::steady_clock::now();
    RecordBuildCost(std::chrono::duration<float, std::milli>(costEnd - costStart).count());
}

// -------------------------------------------------------------
// 작업 스레드가 끝낸 청크를 받아 GPU 버퍼로 만든다. (메인 스레드, S65)
//  버퍼 생성도 공짜는 아니므로 한 프레임에 올리는 개수를 제한한다.
// -------------------------------------------------------------
void ChunkedTerrainRenderer::ReceiveBuiltChunks()
{
    if (!m_graphics || !m_graphics->GetDevice())
        return;

    std::vector<terrain::ChunkBuildResult> results;
    m_worker.TakeCompleted(results, static_cast<size_t>(m_uploadBudget));

    for (terrain::ChunkBuildResult& result : results)
    {
        // 지형 설정이 바뀌기 전에 주문한 결과다. 다른 지형의 조각이므로 버린다.
        if (result.generation != m_generation)
            continue;

        m_inFlight.erase(MakeChunkKey(result.worldX, result.worldZ));

        if (!result.ok)
            continue;

        // 계산하는 사이 카메라가 멀리 가 버렸으면 더는 필요 없다.
        const int slot = SlotForWorldChunk(result.worldX, result.worldZ);
        if (slot < 0)
            continue;

        ChunkSlot& state = m_slots[static_cast<size_t>(slot)];
        if (state.valid && state.worldX == result.worldX && state.worldZ == result.worldZ)
            continue;

        const bool ok = m_chunks[static_cast<size_t>(slot)].Upload(m_graphics->GetDevice(), result.data);
        state = ChunkSlot{ result.worldX, result.worldZ, ok };
        ++m_rebuiltThisFrame;
    }
}

// 월드 청크 좌표가 지금 카메라 기준으로 몇 번 슬롯에 들어가야 하는지. 범위 밖이면 -1.
int ChunkedTerrainRenderer::SlotForWorldChunk(int worldX, int worldZ) const
{
    const int cx = worldX - m_centerChunkX + m_chunksX / 2;
    const int cz = m_chunksZ / 2 - (worldZ - m_centerChunkZ);

    if (cx < 0 || cx >= m_chunksX || cz < 0 || cz >= m_chunksZ)
        return -1;

    return cz * m_chunksX + cx;
}

// 두 정수 좌표를 64비트 키 하나로 합친다. 위 32비트 x, 아래 32비트 z.
uint64_t ChunkedTerrainRenderer::MakeChunkKey(int worldX, int worldZ)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(worldX)) << 32) |
           static_cast<uint64_t>(static_cast<uint32_t>(worldZ));
}

void ChunkedTerrainRenderer::SetAsyncBuildEnabled(bool enabled)
{
    if (m_asyncBuild == enabled)
        return;

    m_asyncBuild = enabled;

    // 방식을 바꾸는 순간 주문 장부를 비운다. 계산 중이던 결과는 세대가 달라 도착해도 버려진다.
    m_worker.CancelPending();
    m_inFlight.clear();
    ++m_generation;
}

// 값이 프레임마다 튀므로 최근 0.5초 동안의 최댓값을 보여 준다.
// 화면이 끊겨 보이는 것은 평균이 아니라 "가장 느린 프레임" 때문이다.
void ChunkedTerrainRenderer::RecordBuildCost(float milliseconds)
{
    m_peakBuildMs = (std::max)(m_peakBuildMs, milliseconds);
    m_peakTimer += TimeManager::Get().GetDeltaTime();

    if (m_peakTimer >= 0.5f)
    {
        m_displayBuildMs = m_peakBuildMs;
        m_peakBuildMs = 0.0f;
        m_peakTimer = 0.0f;
    }
}

float ChunkedTerrainRenderer::GetStreamingRadius() const
{
    // 카메라는 가운데 청크 안 어디든 있을 수 있으므로 한 칸을 빼고 계산한다.
    const float chunkSize = m_cellsPerChunk * m_cellSize;
    const int halfChunks = (std::min)(m_chunksX, m_chunksZ) / 2;
    return static_cast<float>((std::max)(1, halfChunks - 1)) * chunkSize;
}

// -------------------------------------------------------------
// 지면 높이 (S66)
//  보이는 삼각형과 같은 높이를 돌려준다. LOD 로 성겨진 먼 청크와는 조금 다를 수 있지만
//  카메라가 서 있는 발밑은 항상 LOD 0 이라 문제가 없다.
// -------------------------------------------------------------
bool ChunkedTerrainRenderer::TryGetGroundHeight(float x, float z, float& outHeight) const
{
    XMFLOAT3 offset(0.0f, 0.0f, 0.0f);
    if (Transform* transform = GetTransform())
        offset = transform->GetWorldPosition();   // 이동만 반영한다 (회전·확대한 지형은 다루지 않는다)

    const float localX = x - offset.x;
    const float localZ = z - offset.z;

    // 유한 지형은 청크가 깔린 범위 밖이면 땅이 없다. 무한 지형은 어디든 있다.
    if (!m_infiniteEnabled)
    {
        const float chunkSize = m_cellsPerChunk * m_cellSize;
        const float minX = (m_centerChunkX - m_chunksX / 2) * chunkSize;
        const float maxX = minX + m_chunksX * chunkSize;
        const float maxZ = (m_centerChunkZ + m_chunksZ / 2 + 1) * chunkSize;
        const float minZ = maxZ - m_chunksZ * chunkSize;

        if (localX < minX || localX > maxX || localZ < minZ || localZ > maxZ)
            return false;
    }

    // 청크 원점은 모두 칸 크기의 배수라 월드 원점(0, 0)을 격자 기준점으로 쓸 수 있다.
    outHeight = terrain::SampleGridSurface(m_height, 0.0f, 0.0f, m_cellSize, localX, localZ) + offset.y;
    return true;
}

// 무한 지형에서는 청크가 계속 움직이므로 쿼드트리를 매번 다시 세우기 어렵다.
// 청크 수가 수백 개 수준이라 하나씩 검사해도 충분하다.
void ChunkedTerrainRenderer::CullChunksDirectly(const Frustum& frustum)
{
    m_visibleChunks.clear();

    for (int i = 0; i < static_cast<int>(m_chunks.size()); ++i)
    {
        if (!m_chunks[i].IsValid())
            continue;

        if (frustum.IntersectsAABB(m_chunks[i].GetCenter(), m_chunks[i].GetExtents()))
            m_visibleChunks.push_back(i);
    }

    m_stats = terrain::TerrainQuadTree::Stats{};
    m_stats.totalChunks = static_cast<int>(m_chunks.size());
    m_stats.visibleChunks = static_cast<int>(m_visibleChunks.size());
    m_stats.nodesTested = static_cast<int>(m_chunks.size());
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
// 지오모핑 계수 (S53)
//  LOD 가 바뀌는 거리에 가까워질수록 0 -> 1 로 올라간다.
//  전환 순간에 1 이 되므로, 인덱스를 바꿔 끼울 때는 이미 모양이 같다.
// -------------------------------------------------------------
float ChunkedTerrainRenderer::SelectMorph(const XMFLOAT3& chunkCenter, const XMFLOAT3& eye) const
{
    if (!m_lodEnabled || !m_morphEnabled)
        return 0.0f;

    const float dx = chunkCenter.x - eye.x;
    const float dy = chunkCenter.y - eye.y;
    const float dz = chunkCenter.z - eye.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    const float band = distance / m_lodDistance;
    const float fraction = band - std::floor(band);

    // 구간의 뒤쪽 40% 에서만 서서히 옮긴다. 너무 일찍 시작하면 항상 뭉개져 보인다.
    const float start = 0.6f;
    if (fraction <= start)
        return 0.0f;

    const float t = (fraction - start) / (1.0f - start);
    return t * t * (3.0f - 2.0f * t);   // smoothstep
}

void ChunkedTerrainRenderer::SetSkirtEnabled(bool enabled)
{
    if (m_skirtEnabled == enabled)
        return;

    m_skirtEnabled = enabled;
    m_dirty = true;   // 스커트는 메시에 들어 있으므로 다시 만들어야 한다
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

    // 무한 지형이면 카메라를 따라 청크를 재활용한다.
    if (m_infiniteEnabled)
        UpdateInfiniteChunks(eye);
    else
        m_rebuiltThisFrame = 0;

    if (m_cullingEnabled && m_infiniteEnabled)
    {
        Frustum frustum;
        frustum.BuildFromViewProjection(m_graphics->GetView3D() * m_graphics->GetProjection3D());
        CullChunksDirectly(frustum);
    }
    else if (m_cullingEnabled && m_quadTree.IsValid())
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
    if (m_chunkMorph.size() != m_chunks.size())
        m_chunkMorph.assign(m_chunks.size(), 0.0f);

    for (int index : m_visibleChunks)
    {
        if (index < 0 || index >= static_cast<int>(m_chunks.size()))
            continue;

        const XMFLOAT3& center = m_chunks[index].GetCenter();
        m_chunkLod[index] = SelectLod(center, eye);
        m_chunkMorph[index] = SelectMorph(center, eye);
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

        // 청크별이 아니라 지형 전체 기준으로 정규화한다.
        draw.heightRange = XMFLOAT4(m_globalMinHeight, m_globalMaxHeight, 0.0f, 0.0f);
        const float morph = m_chunkMorph[index];
        draw.splat = XMFLOAT4(6.0f, splat ? 1.0f : 0.0f, debugColor ? 1.0f : 0.0f, morph);

        // 트라이플래너 (S63) : 청크 하나에 6장 반복하던 것과 같은 크기로 월드 좌표에서 찍는다.
        draw.surface = XMFLOAT4(m_triplanarEnabled ? 1.0f : 0.0f,
                                (m_cellsPerChunk * m_cellSize) / 6.0f,
                                4.0f, 0.0f);

        // 현재 LOD 에 해당하는 모프 타깃만 고르도록 성분 하나만 1 로 둔다.
        draw.lodSelect = XMFLOAT4(lod == 0 ? 1.0f : 0.0f,
                                  lod == 1 ? 1.0f : 0.0f,
                                  lod == 2 ? 1.0f : 0.0f,
                                  lod == 3 ? 1.0f : 0.0f);

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
