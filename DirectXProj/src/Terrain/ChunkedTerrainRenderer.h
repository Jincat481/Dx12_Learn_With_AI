#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Texture.h"
#include "Terrain/TerrainChunk.h"
#include "Terrain/TerrainQuadTree.h"
#include "Terrain/HeightField.h"
#include "Terrain/ChunkBuildWorker.h"
#include "Engine/GroundProvider.h"

#include <unordered_set>

class Graphics;

// =============================================================
// ChunkedTerrainRenderer (스텝 5, 6)
//  지형을 청크로 쪼개 쿼드트리에 담고, 보이는 것만 그린다.
//
//  스텝 1~4 의 TerrainRenderer 는 지형을 메시 한 덩어리로 만든다.
//  그 방식은 화면에 한 귀퉁이만 보여도 전체를 그려야 한다.
//  여기서는 타일마다 따로 판단한다.
//
//   - 절두체 컬링 : 보이지 않는 청크는 Draw 호출 자체를 하지 않는다 (스텝 5)
//   - 거리 LOD    : 먼 청크는 인덱스 간격을 벌려 삼각형을 줄인다 (스텝 6)
//   - 백그라운드 생성 : 무한 지형의 새 청크를 작업 스레드가 계산한다 (S65)
// =============================================================
class ChunkedTerrainRenderer : public Component, public IGroundProvider
{
public:
    // 무엇을 보여줄지
    enum class DisplayMode
    {
        Splatting,     // 텍스처 스플래팅
        HeightColor,   // 높이 색상
        ChunkColor,    // 청크마다 다른 색 (분할이 눈에 보이게)
        LodColor,      // LOD 단계마다 다른 색
        Wireframe,     // 와이어프레임
        Count
    };

    ChunkedTerrainRenderer() = default;
    ~ChunkedTerrainRenderer() override = default;

    const char* GetTypeName() const override { return "ChunkedTerrainRenderer"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;
    void Render() override;
    void OnDestroy() override;

    void SetGrid(int chunksX, int chunksZ, int cellsPerChunk, float cellSize);
    void SetHeightParams(const terrain::HeightParams& params);
    void Regenerate(unsigned seed);

    void SetDisplayMode(DisplayMode mode);
    void CycleDisplayMode();
    DisplayMode GetDisplayMode() const { return m_displayMode; }
    const wchar_t* GetDisplayModeName() const;

    void SetCullingEnabled(bool enabled) { m_cullingEnabled = enabled; }
    bool IsCullingEnabled() const { return m_cullingEnabled; }
    void ToggleCulling() { m_cullingEnabled = !m_cullingEnabled; }

    // ---- 스텝 6-2 : 스커트 + 지오모핑 ----
    void SetSkirtEnabled(bool enabled);
    bool IsSkirtEnabled() const { return m_skirtEnabled; }
    void ToggleSkirt() { SetSkirtEnabled(!m_skirtEnabled); }

    void SetMorphEnabled(bool enabled) { m_morphEnabled = enabled; }
    bool IsMorphEnabled() const { return m_morphEnabled; }
    void ToggleMorph() { m_morphEnabled = !m_morphEnabled; }

    // ---- 스텝 10 : 무한 지형 ----
    //  카메라가 청크 한 칸을 넘어갈 때마다, 뒤로 밀려난 청크를
    //  반대쪽 좌표로 다시 만들어 재활용한다. 배열 크기는 그대로다.
    void SetInfiniteEnabled(bool enabled);
    bool IsInfiniteEnabled() const { return m_infiniteEnabled; }
    void ToggleInfinite() { SetInfiniteEnabled(!m_infiniteEnabled); }

    int GetRebuiltThisFrame() const { return m_rebuiltThisFrame; }

    // ---- 보강 : 트라이플래너 (S63) ----
    void SetTriplanarEnabled(bool enabled) { m_triplanarEnabled = enabled; }
    bool IsTriplanarEnabled() const { return m_triplanarEnabled; }
    void ToggleTriplanar() { m_triplanarEnabled = !m_triplanarEnabled; }

    // ---- 보강 : 백그라운드 생성 (S65) ----
    //  켜면 새 청크의 정점 계산은 작업 스레드가 하고, 메인 스레드는 버퍼만 만든다.
    //  끄면 예전처럼 메인 스레드가 프레임당 몇 개씩 직접 만든다(비교용).
    void SetAsyncBuildEnabled(bool enabled);
    bool IsAsyncBuildEnabled() const { return m_asyncBuild; }
    void ToggleAsyncBuild() { SetAsyncBuildEnabled(!m_asyncBuild); }

    int    GetWorkerThreadCount() const { return m_worker.GetThreadCount(); }
    size_t GetOutstandingBuildCount() const { return m_worker.GetOutstandingCount(); }
    float  GetMainThreadBuildMs() const { return m_displayBuildMs; }   // 최근 0.5초 중 가장 오래 걸린 프레임

    // 무한 지형에서 카메라가 어디 있든 지형이 있다고 보장되는 거리. 안개 끝 거리를 여기에 맞춘다.
    float GetStreamingRadius() const;

    // ---- 보강 : 지면 높이 (S66) ----
    bool TryGetGroundHeight(float x, float z, float& outHeight) const override;

    // ---- 에디터 : 편집 (S69) ----
    //  노이즈를 격자로 구워 브러시로 고칠 수 있게 한다. 유한 지형 전용이다.
    void EnableEditing();
    bool IsEditable() const { return static_cast<bool>(m_editGrid); }
    terrain::HeightGrid* GetEditGrid() const { return m_editGrid.get(); }

    // 월드 사각 영역의 높이가 바뀌었다. 겹치는 청크만 다음 Update 에서 다시 만든다.
    void MarkRegionDirty(float minX, float minZ, float maxX, float maxZ);
    int   GetLastEditRebuildCount() const { return m_lastEditRebuildCount; }
    float GetLastEditRebuildMs() const { return m_lastEditRebuildMs; }

    // 브러시 미리보기 원. 셰이더가 지형 표면에 그린다. (S68)
    void SetBrushPreview(bool visible, float x, float z, float radius, int tool);

    // ---- 저장 / 불러오기 (S70) ----
    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

    void SetLodEnabled(bool enabled) { m_lodEnabled = enabled; }
    bool IsLodEnabled() const { return m_lodEnabled; }
    void ToggleLod() { m_lodEnabled = !m_lodEnabled; }

    // 화면에 띄울 통계
    const terrain::TerrainQuadTree::Stats& GetStats() const { return m_stats; }
    int GetDrawnTriangles() const { return m_drawnTriangles; }

private:
    bool RebuildChunks();
    void LoadSplatLayers();
    void UpdateInfiniteChunks(const DirectX::XMFLOAT3& eye);
    bool BuildChunkAt(size_t slot, int worldChunkX, int worldChunkZ);
    void CullChunksDirectly(const Frustum& frustum);

    void ReceiveBuiltChunks();
    void RebuildDirtyChunks();
    void RecomputeGlobalHeightRange();
    void UseEditGrid(std::shared_ptr<terrain::HeightGrid> grid);
    int  SlotForWorldChunk(int worldX, int worldZ) const;
    void RecordBuildCost(float milliseconds);
    static uint64_t MakeChunkKey(int worldX, int worldZ);

    int  SelectLod(const DirectX::XMFLOAT3& chunkCenter, const DirectX::XMFLOAT3& eye) const;
    float SelectMorph(const DirectX::XMFLOAT3& chunkCenter, const DirectX::XMFLOAT3& eye) const;
    DirectX::XMFLOAT4 GetDebugColor(int chunkIndex, int lod) const;

    Graphics* m_graphics = nullptr;

    std::vector<terrain::TerrainChunk> m_chunks;
    terrain::TerrainQuadTree m_quadTree;
    terrain::HeightField m_height;

    // 이번 프레임에 보이는 청크와 각자의 LOD
    std::vector<int> m_visibleChunks;
    std::vector<int> m_chunkLod;
    std::vector<float> m_chunkMorph;
    terrain::TerrainQuadTree::Stats m_stats;

    // 색상 램프는 지형 전체 기준으로 정규화해야 한다.
    // 청크마다 자기 min/max 를 쓰면 평평한 청크에도 눈이 덮인다.
    float m_globalMinHeight = 0.0f;
    float m_globalMaxHeight = 1.0f;
    int m_drawnTriangles = 0;

    int   m_chunksX = 8;
    int   m_chunksZ = 8;
    int   m_cellsPerChunk = 16;
    float m_cellSize = 2.0f;

    bool m_dirty = true;
    bool m_cullingEnabled = true;
    bool m_lodEnabled = false;
    bool m_skirtEnabled = true;
    bool m_morphEnabled = true;

    // 무한 지형 : 슬롯마다 지금 담당하는 월드 청크 좌표를 기억한다.
    struct ChunkSlot { int worldX = 0; int worldZ = 0; bool valid = false; };
    std::vector<ChunkSlot> m_slots;
    bool m_infiniteEnabled = false;
    int  m_centerChunkX = 0;
    int  m_centerChunkZ = 0;
    int  m_rebuildBudget = 3;      // 한 프레임에 다시 만들 청크 수 상한
    int  m_rebuiltThisFrame = 0;

    // 백그라운드 생성 (S65)
    terrain::ChunkBuildWorker m_worker;
    std::shared_ptr<const terrain::HeightField> m_heightSnapshot;   // 작업 스레드에 넘기는 높이 함수 사본
    std::unordered_set<uint64_t> m_inFlight;   // 주문했지만 아직 받지 않은 청크 좌표
    uint32_t m_generation = 0;                 // 지형 설정이 바뀔 때마다 올린다
    bool  m_asyncBuild = true;
    int   m_uploadBudget = 6;                  // 한 프레임에 GPU 로 올릴 청크 수 상한
    int   m_lastCenterX = 0;
    int   m_lastCenterZ = 0;
    bool  m_hasLastCenter = false;
    float m_peakBuildMs = 0.0f;
    float m_displayBuildMs = 0.0f;
    float m_peakTimer = 0.0f;

    bool m_triplanarEnabled = true;            // 보강 (S63)

    // 편집 (S69)
    std::shared_ptr<terrain::HeightGrid> m_editGrid;
    terrain::HeightParams m_baseParams;        // 격자를 구울 때 쓴 노이즈 설정 (N 으로 다시 구울 때 쓴다)
    std::vector<uint8_t> m_dirtyChunks;        // 다시 만들 청크 표시
    bool  m_anyDirtyChunk = false;
    int   m_lastEditRebuildCount = 0;
    float m_lastEditRebuildMs = 0.0f;
    DirectX::XMFLOAT4 m_brushPreview{ 0.0f, 0.0f, 0.0f, 0.0f };

    DisplayMode m_displayMode = DisplayMode::ChunkColor;

    float m_lodDistance = 90.0f;   // 이 거리마다 LOD 한 단계씩 내린다

    static constexpr int kLayerCount = 4;
    std::shared_ptr<Texture> m_layers[kLayerCount];

    DirectX::XMFLOAT4 m_wireColor{ 0.55f, 0.75f, 0.95f, 1.0f };
};
