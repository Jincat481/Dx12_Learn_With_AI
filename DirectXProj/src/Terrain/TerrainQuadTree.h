#pragma once
#include "Core/stdafx.h"
#include "Core/Frustum.h"
#include "Terrain/TerrainChunk.h"

// =============================================================
// TerrainQuadTree (S50)
//  청크를 4등분 트리로 묶는다.
//
//  청크가 64개면 하나씩 절두체 검사를 해도 64번이면 된다.
//  하지만 청크가 수천 개가 되면 그것만으로도 부담이다.
//  4등분 트리로 묶어 두면 **부모 상자가 화면 밖이면 그 아래 전부를 한 번에 버린다.**
//  검사 횟수가 O(n) 에서 대략 O(log n + 보이는 개수) 로 줄어든다.
//
//      ┌───┬───┐   루트가 화면 밖 → 자식 4개 검사도 생략
//      │ 0 │ 1 │   루트가 걸침    → 자식 4개를 각각 검사
//      ├───┼───┤
//      │ 2 │ 3 │
//      └───┴───┘
// =============================================================
namespace terrain
{
    class TerrainQuadTree
    {
    public:
        struct Stats
        {
            int visibleChunks = 0;
            int totalChunks = 0;
            int nodesTested = 0;    // 절두체 검사를 실제로 한 노드 수
            int nodesCulled = 0;    // 통째로 버린 노드 수(그 아래 전부 생략)
        };

        void Build(int chunksX, int chunksZ, const std::vector<TerrainChunk>& chunks);
        void Clear();

        bool IsValid() const { return !m_nodes.empty(); }

        // 절두체와 겹치는 청크 인덱스를 모은다.
        void Cull(const Frustum& frustum, std::vector<int>& outVisible, Stats& outStats) const;

        int GetTotalChunks() const { return m_totalChunks; }

    private:
        struct Node
        {
            DirectX::XMFLOAT3 center{};
            DirectX::XMFLOAT3 extents{};
            int child[4] = { -1, -1, -1, -1 };
            int chunkIndex = -1;      // 리프일 때만 유효
        };

        int  BuildNode(int x0, int z0, int x1, int z1,
                       int chunksX, const std::vector<TerrainChunk>& chunks);

        void CullNode(int nodeIndex, const Frustum& frustum,
                      std::vector<int>& outVisible, Stats& outStats) const;

        std::vector<Node> m_nodes;
        int m_rootIndex = -1;
        int m_totalChunks = 0;
    };
}
