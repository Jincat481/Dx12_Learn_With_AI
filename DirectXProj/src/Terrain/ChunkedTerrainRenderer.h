#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Texture.h"
#include "Terrain/TerrainChunk.h"
#include "Terrain/TerrainQuadTree.h"
#include "Terrain/HeightField.h"

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
// =============================================================
class ChunkedTerrainRenderer : public Component
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

    void SetLodEnabled(bool enabled) { m_lodEnabled = enabled; }
    bool IsLodEnabled() const { return m_lodEnabled; }
    void ToggleLod() { m_lodEnabled = !m_lodEnabled; }

    // 화면에 띄울 통계
    const terrain::TerrainQuadTree::Stats& GetStats() const { return m_stats; }
    int GetDrawnTriangles() const { return m_drawnTriangles; }

private:
    bool RebuildChunks();
    void LoadSplatLayers();
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

    DisplayMode m_displayMode = DisplayMode::ChunkColor;

    float m_lodDistance = 90.0f;   // 이 거리마다 LOD 한 단계씩 내린다

    static constexpr int kLayerCount = 4;
    std::shared_ptr<Texture> m_layers[kLayerCount];

    DirectX::XMFLOAT4 m_wireColor{ 0.55f, 0.75f, 0.95f, 1.0f };
};
