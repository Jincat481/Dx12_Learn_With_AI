#include "Core/stdafx.h"
#include "Terrain/TerrainChunk.h"

#include <limits>

using namespace DirectX;

namespace terrain
{
    bool TerrainChunk::Build(ID3D11Device* device,
                             float originX, float originZ,
                             int cells, float cellSize,
                             const HeightField* height)
    {
        if (!device || cells <= 0 || cellSize <= 0.0f)
            return false;

        const int cols = cells + 1;   // 한 변의 정점 수

        // ---- 정점 ----
        //  LOD 는 인덱스만 바꿔 만들 것이므로 정점은 가장 촘촘한 한 벌만 만든다.
        std::vector<TerrainVertex> vertices;
        vertices.reserve(static_cast<size_t>(cols) * cols);

        m_minHeight = (std::numeric_limits<float>::max)();
        m_maxHeight = -(std::numeric_limits<float>::max)();

        for (int r = 0; r < cols; ++r)
        {
            // r 이 커질수록 -Z 로 내려간다 (전체 격자와 같은 규약)
            const float z = originZ - r * cellSize;

            for (int c = 0; c < cols; ++c)
            {
                const float x = originX + c * cellSize;

                TerrainVertex vertex;
                if (height)
                {
                    vertex.position = XMFLOAT3(x, height->Sample(x, z), z);
                    vertex.normal = height->SampleNormal(x, z, cellSize);
                }
                else
                {
                    vertex.position = XMFLOAT3(x, 0.0f, z);
                    vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
                }

                // UV 는 지형 전체 기준이 아니라 청크 안에서 0~1 로 둔다.
                // 스플래팅은 타일링해서 쓰므로 이어 붙여도 티가 나지 않는다.
                vertex.uv = XMFLOAT2(static_cast<float>(c) / cells,
                                     static_cast<float>(r) / cells);

                m_minHeight = (std::min)(m_minHeight, vertex.position.y);
                m_maxHeight = (std::max)(m_maxHeight, vertex.position.y);

                vertices.push_back(vertex);
            }
        }

        // ---- 인덱스 : LOD 단계별로 이어 붙인다 ----
        //  stride 를 2배씩 벌리면 삼각형 수가 1/4 로 줄어든다.
        std::vector<uint32_t> indices;

        m_lodCount = 0;
        for (int lod = 0; lod < kMaxLod; ++lod)
        {
            const int stride = 1 << lod;
            if (cells % stride != 0 || (cells / stride) < 1)
                break;

            m_indexOffset[lod] = static_cast<UINT>(indices.size());

            const int steps = cells / stride;
            for (int r = 0; r < steps; ++r)
            {
                for (int c = 0; c < steps; ++c)
                {
                    const int r0 = r * stride;
                    const int c0 = c * stride;
                    const int r1 = r0 + stride;
                    const int c1 = c0 + stride;

                    const uint32_t topLeft     = static_cast<uint32_t>(r0 * cols + c0);
                    const uint32_t topRight    = static_cast<uint32_t>(r0 * cols + c1);
                    const uint32_t bottomLeft  = static_cast<uint32_t>(r1 * cols + c0);
                    const uint32_t bottomRight = static_cast<uint32_t>(r1 * cols + c1);

                    indices.push_back(topLeft);
                    indices.push_back(topRight);
                    indices.push_back(bottomLeft);

                    indices.push_back(bottomLeft);
                    indices.push_back(topRight);
                    indices.push_back(bottomRight);
                }
            }

            m_indexCount[lod] = static_cast<UINT>(indices.size()) - m_indexOffset[lod];
            ++m_lodCount;
        }

        if (m_lodCount == 0 || indices.empty())
            return false;

        // ---- 경계 상자 ----
        const float half = cells * cellSize * 0.5f;
        m_center = XMFLOAT3(originX + half,
                            (m_minHeight + m_maxHeight) * 0.5f,
                            originZ - half);
        m_extents = XMFLOAT3(half,
                             (std::max)((m_maxHeight - m_minHeight) * 0.5f, 0.01f),
                             half);

        return m_mesh.Create(device,
                             vertices.data(), static_cast<UINT>(vertices.size()),
                             static_cast<UINT>(sizeof(TerrainVertex)),
                             indices.data(), static_cast<UINT>(indices.size()));
    }

    void TerrainChunk::Release()
    {
        m_mesh.Release();
        m_lodCount = 1;
    }

    UINT TerrainChunk::GetIndexOffset(int lod) const
    {
        lod = (std::max)(0, (std::min)(m_lodCount - 1, lod));
        return m_indexOffset[lod];
    }

    UINT TerrainChunk::GetIndexCount(int lod) const
    {
        lod = (std::max)(0, (std::min)(m_lodCount - 1, lod));
        return m_indexCount[lod];
    }
}
