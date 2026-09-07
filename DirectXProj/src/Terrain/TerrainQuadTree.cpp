#include "Core/stdafx.h"
#include "Terrain/TerrainQuadTree.h"

using namespace DirectX;

namespace terrain
{
    namespace
    {
        // 두 AABB 를 모두 감싸는 AABB
        void MergeAABB(XMFLOAT3& center, XMFLOAT3& extents,
                       const XMFLOAT3& otherCenter, const XMFLOAT3& otherExtents,
                       bool first)
        {
            if (first)
            {
                center = otherCenter;
                extents = otherExtents;
                return;
            }

            const float minX = (std::min)(center.x - extents.x, otherCenter.x - otherExtents.x);
            const float maxX = (std::max)(center.x + extents.x, otherCenter.x + otherExtents.x);
            const float minY = (std::min)(center.y - extents.y, otherCenter.y - otherExtents.y);
            const float maxY = (std::max)(center.y + extents.y, otherCenter.y + otherExtents.y);
            const float minZ = (std::min)(center.z - extents.z, otherCenter.z - otherExtents.z);
            const float maxZ = (std::max)(center.z + extents.z, otherCenter.z + otherExtents.z);

            center = XMFLOAT3((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f);
            extents = XMFLOAT3((maxX - minX) * 0.5f, (maxY - minY) * 0.5f, (maxZ - minZ) * 0.5f);
        }
    }

    void TerrainQuadTree::Clear()
    {
        m_nodes.clear();
        m_rootIndex = -1;
        m_totalChunks = 0;
    }

    void TerrainQuadTree::Build(int chunksX, int chunksZ, const std::vector<TerrainChunk>& chunks)
    {
        Clear();

        if (chunksX <= 0 || chunksZ <= 0 || chunks.empty())
            return;

        m_totalChunks = static_cast<int>(chunks.size());
        m_nodes.reserve(chunks.size() * 2);

        m_rootIndex = BuildNode(0, 0, chunksX, chunksZ, chunksX, chunks);

        dxutil::DebugLog(L"[QuadTree] 노드 %zu개 / 청크 %d개", m_nodes.size(), m_totalChunks);
    }

    // [x0, x1) x [z0, z1) 범위를 담당하는 노드를 만든다.
    int TerrainQuadTree::BuildNode(int x0, int z0, int x1, int z1,
                                   int chunksX, const std::vector<TerrainChunk>& chunks)
    {
        const int width = x1 - x0;
        const int depth = z1 - z0;

        if (width <= 0 || depth <= 0)
            return -1;

        const int nodeIndex = static_cast<int>(m_nodes.size());
        m_nodes.push_back(Node{});

        // ---- 리프 : 청크 하나 ----
        if (width == 1 && depth == 1)
        {
            const int chunkIndex = z0 * chunksX + x0;
            if (chunkIndex < 0 || chunkIndex >= static_cast<int>(chunks.size()))
                return nodeIndex;

            Node& leaf = m_nodes[nodeIndex];
            leaf.chunkIndex = chunkIndex;
            leaf.center = chunks[chunkIndex].GetCenter();
            leaf.extents = chunks[chunkIndex].GetExtents();
            return nodeIndex;
        }

        // ---- 내부 노드 : 4등분 ----
        const int midX = x0 + (width + 1) / 2;
        const int midZ = z0 + (depth + 1) / 2;

        const int childIndices[4][4] =
        {
            { x0,   z0,   midX, midZ },
            { midX, z0,   x1,   midZ },
            { x0,   midZ, midX, z1   },
            { midX, midZ, x1,   z1   },
        };

        bool first = true;
        XMFLOAT3 center{};
        XMFLOAT3 extents{};

        for (int i = 0; i < 4; ++i)
        {
            const int cx0 = childIndices[i][0];
            const int cz0 = childIndices[i][1];
            const int cx1 = childIndices[i][2];
            const int cz1 = childIndices[i][3];

            if (cx1 <= cx0 || cz1 <= cz0)
                continue;

            const int childIndex = BuildNode(cx0, cz0, cx1, cz1, chunksX, chunks);
            if (childIndex < 0)
                continue;

            m_nodes[nodeIndex].child[i] = childIndex;

            // 자식 상자를 합쳐 부모 상자를 만든다.
            MergeAABB(center, extents, m_nodes[childIndex].center, m_nodes[childIndex].extents, first);
            first = false;
        }

        m_nodes[nodeIndex].center = center;
        m_nodes[nodeIndex].extents = extents;
        return nodeIndex;
    }

    void TerrainQuadTree::Cull(const Frustum& frustum, std::vector<int>& outVisible, Stats& outStats) const
    {
        outVisible.clear();
        outStats = Stats{};
        outStats.totalChunks = m_totalChunks;

        if (m_rootIndex >= 0)
            CullNode(m_rootIndex, frustum, outVisible, outStats);

        outStats.visibleChunks = static_cast<int>(outVisible.size());
    }

    void TerrainQuadTree::CullNode(int nodeIndex, const Frustum& frustum,
                                   std::vector<int>& outVisible, Stats& outStats) const
    {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_nodes.size()))
            return;

        const Node& node = m_nodes[nodeIndex];
        ++outStats.nodesTested;

        // 여기서 걸러지면 이 아래 전부를 검사하지 않는다. 이게 쿼드트리의 이득이다.
        if (!frustum.IntersectsAABB(node.center, node.extents))
        {
            ++outStats.nodesCulled;
            return;
        }

        if (node.chunkIndex >= 0)
        {
            outVisible.push_back(node.chunkIndex);
            return;
        }

        for (int i = 0; i < 4; ++i)
            CullNode(node.child[i], frustum, outVisible, outStats);
    }
}
