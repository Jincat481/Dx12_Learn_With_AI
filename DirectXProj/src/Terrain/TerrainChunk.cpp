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
        if (!device)
            return false;

        ChunkMeshData data;
        if (!BuildMeshData(originX, originZ, cells, cellSize, m_skirtEnabled, height, data))
            return false;

        return Upload(device, data);
    }

    // -------------------------------------------------------------
    // 1단계 : CPU 계산
    //  여기서는 멤버 변수를 하나도 쓰지 않는다. 입력은 인자, 출력은 out 뿐이다.
    //  그래서 여러 작업 스레드가 동시에 불러도 서로 부딪히지 않는다. (S65)
    //  HeightField::Sample 도 const 이고 내부 상태를 바꾸지 않는다.
    // -------------------------------------------------------------
    bool TerrainChunk::BuildMeshData(float originX, float originZ,
                                     int cells, float cellSize, bool skirt,
                                     const HeightField* height,
                                     ChunkMeshData& out)
    {
        if (cells <= 0 || cellSize <= 0.0f)
            return false;

        const int cols = cells + 1;   // 한 변의 정점 수

        // ---- 1) 높이를 먼저 전부 구해 둔다 ----
        //  모프 타깃(다음 LOD 에서의 높이)을 계산하려면 이웃 높이가 필요하다.
        std::vector<float> heights(static_cast<size_t>(cols) * cols, 0.0f);

        out.minHeight = (std::numeric_limits<float>::max)();
        out.maxHeight = -(std::numeric_limits<float>::max)();

        for (int r = 0; r < cols; ++r)
        {
            const float z = originZ - r * cellSize;
            for (int c = 0; c < cols; ++c)
            {
                const float x = originX + c * cellSize;
                const float y = height ? height->Sample(x, z) : 0.0f;

                heights[static_cast<size_t>(r) * cols + c] = y;
                out.minHeight = (std::min)(out.minHeight, y);
                out.maxHeight = (std::max)(out.maxHeight, y);
            }
        }

        // 격자 안의 임의 위치 높이를 이중선형으로 구한다(모프 타깃 계산용).
        auto sampleHeight = [&](float fr, float fc) -> float
        {
            fr = (std::max)(0.0f, (std::min)(static_cast<float>(cols - 1), fr));
            fc = (std::max)(0.0f, (std::min)(static_cast<float>(cols - 1), fc));

            const int r0 = static_cast<int>(fr);
            const int c0 = static_cast<int>(fc);
            const int r1 = (std::min)(cols - 1, r0 + 1);
            const int c1 = (std::min)(cols - 1, c0 + 1);

            const float tr = fr - r0;
            const float tc = fc - c0;

            const float h00 = heights[static_cast<size_t>(r0) * cols + c0];
            const float h01 = heights[static_cast<size_t>(r0) * cols + c1];
            const float h10 = heights[static_cast<size_t>(r1) * cols + c0];
            const float h11 = heights[static_cast<size_t>(r1) * cols + c1];

            const float a = h00 + (h01 - h00) * tc;
            const float b = h10 + (h11 - h10) * tc;
            return a + (b - a) * tr;
        };

        // ---- 2) 정점 ----
        //  LOD 는 인덱스만 바꿔 만들 것이므로 정점은 가장 촘촘한 한 벌만 만든다.
        std::vector<TerrainVertex>& vertices = out.vertices;
        vertices.clear();
        vertices.reserve(static_cast<size_t>(cols) * cols + static_cast<size_t>(cells) * 4);

        for (int r = 0; r < cols; ++r)
        {
            // r 이 커질수록 -Z 로 내려간다 (전체 격자와 같은 규약)
            const float z = originZ - r * cellSize;

            for (int c = 0; c < cols; ++c)
            {
                const float x = originX + c * cellSize;

                TerrainVertex vertex;
                vertex.position = XMFLOAT3(x, heights[static_cast<size_t>(r) * cols + c], z);
                vertex.normal = height ? height->SampleNormal(x, z, cellSize)
                                       : XMFLOAT3(0.0f, 1.0f, 0.0f);

                // UV 는 지형 전체 기준이 아니라 청크 안에서 0~1 로 둔다.
                // 스플래팅은 타일링해서 쓰므로 이어 붙여도 티가 나지 않는다.
                vertex.uv = XMFLOAT2(static_cast<float>(c) / cells,
                                     static_cast<float>(r) / cells);

                // ---- 모프 타깃 (S53) ----
                //  LOD L 에서는 stride 2^L 간격의 정점만 남는다.
                //  사라지는 정점은 주변 남는 정점들 사이 값으로 대체되므로,
                //  그 값을 미리 구해 두면 셰이더에서 부드럽게 옮겨 갈 수 있다.
                float targets[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                for (int level = 0; level < 4; ++level)
                {
                    const int coarse = 1 << (level + 1);

                    // 이 정점을 감싸는 성긴 격자 칸 안에서의 위치
                    const float fr = static_cast<float>((r / coarse) * coarse);
                    const float fc = static_cast<float>((c / coarse) * coarse);
                    const float lr = (std::min)(static_cast<float>(cols - 1), fr + coarse);
                    const float lc = (std::min)(static_cast<float>(cols - 1), fc + coarse);

                    const float tr = (lr > fr) ? (r - fr) / (lr - fr) : 0.0f;
                    const float tc = (lc > fc) ? (c - fc) / (lc - fc) : 0.0f;

                    const float h00 = sampleHeight(fr, fc);
                    const float h01 = sampleHeight(fr, lc);
                    const float h10 = sampleHeight(lr, fc);
                    const float h11 = sampleHeight(lr, lc);

                    const float a = h00 + (h01 - h00) * tc;
                    const float b = h10 + (h11 - h10) * tc;
                    targets[level] = a + (b - a) * tr;
                }
                vertex.morphTargets = XMFLOAT4(targets[0], targets[1], targets[2], targets[3]);

                vertices.push_back(vertex);
            }
        }

        // ---- 3) 스커트 정점 (S52) ----
        //  이웃 청크와 LOD 가 다르면 경계에 틈이 벌어진다.
        //  테두리를 따라 아래로 내려뜨린 "치마" 를 붙여 그 틈을 메운다.
        const float skirtDepth = (std::max)(cellSize * 4.0f, (out.maxHeight - out.minHeight) * 0.3f);

        // 테두리를 한 바퀴 도는 순서 : 위 -> 오른쪽 -> 아래 -> 왼쪽
        std::vector<int> ringVertex;
        ringVertex.reserve(static_cast<size_t>(cells) * 4);

        for (int c = 0; c < cells; ++c) ringVertex.push_back(0 * cols + c);
        for (int r = 0; r < cells; ++r) ringVertex.push_back(r * cols + cells);
        for (int c = cells; c > 0; --c) ringVertex.push_back(cells * cols + c);
        for (int r = cells; r > 0; --r) ringVertex.push_back(r * cols + 0);

        const uint32_t skirtBase = static_cast<uint32_t>(vertices.size());
        for (int gridIndex : ringVertex)
        {
            TerrainVertex skirtVertex = vertices[gridIndex];
            skirtVertex.position.y -= skirtDepth;
            skirtVertex.morphTargets.x -= skirtDepth;
            skirtVertex.morphTargets.y -= skirtDepth;
            skirtVertex.morphTargets.z -= skirtDepth;
            skirtVertex.morphTargets.w -= skirtDepth;
            vertices.push_back(skirtVertex);
        }

        const int ringCount = static_cast<int>(ringVertex.size());

        // ---- 인덱스 : LOD 단계별로 이어 붙인다 ----
        //  stride 를 2배씩 벌리면 삼각형 수가 1/4 로 줄어든다.
        std::vector<uint32_t>& indices = out.indices;
        indices.clear();

        out.lodCount = 0;
        for (int lod = 0; lod < kChunkMaxLod; ++lod)
        {
            const int stride = 1 << lod;
            if (cells % stride != 0 || (cells / stride) < 1)
                break;

            out.indexOffset[lod] = static_cast<UINT>(indices.size());

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

            // 이 LOD 의 테두리 간격에 맞춰 스커트를 두른다.
            if (skirt && ringCount % stride == 0)
            {
                for (int k = 0; k < ringCount; k += stride)
                {
                    const int k1 = (k + stride) % ringCount;

                    const uint32_t top0 = static_cast<uint32_t>(ringVertex[k]);
                    const uint32_t top1 = static_cast<uint32_t>(ringVertex[k1]);
                    const uint32_t bottom0 = skirtBase + static_cast<uint32_t>(k);
                    const uint32_t bottom1 = skirtBase + static_cast<uint32_t>(k1);

                    indices.push_back(top0);
                    indices.push_back(top1);
                    indices.push_back(bottom0);

                    indices.push_back(bottom0);
                    indices.push_back(top1);
                    indices.push_back(bottom1);
                }
            }

            out.indexCount[lod] = static_cast<UINT>(indices.size()) - out.indexOffset[lod];
            ++out.lodCount;
        }

        if (out.lodCount == 0 || indices.empty())
            return false;

        // ---- 경계 상자 ----
        //  스커트가 아래로 내려가므로 상자도 그만큼 넓혀야 컬링에서 잘리지 않는다.
        const float boxMin = out.minHeight - (skirt ? skirtDepth : 0.0f);
        const float half = cells * cellSize * 0.5f;
        out.center = XMFLOAT3(originX + half,
                              (boxMin + out.maxHeight) * 0.5f,
                              originZ - half);
        out.extents = XMFLOAT3(half,
                               (std::max)((out.maxHeight - boxMin) * 0.5f, 0.01f),
                               half);
        return true;
    }

    // -------------------------------------------------------------
    // 2단계 : GPU 업로드 (메인 스레드)
    // -------------------------------------------------------------
    bool TerrainChunk::Upload(ID3D11Device* device, const ChunkMeshData& data)
    {
        if (!device || data.vertices.empty() || data.indices.empty() || data.lodCount == 0)
            return false;

        for (int lod = 0; lod < kMaxLod; ++lod)
        {
            m_indexOffset[lod] = data.indexOffset[lod];
            m_indexCount[lod] = data.indexCount[lod];
        }
        m_lodCount = data.lodCount;

        m_center = data.center;
        m_extents = data.extents;
        m_minHeight = data.minHeight;
        m_maxHeight = data.maxHeight;

        // Mesh::Create 는 이전 버퍼를 먼저 해제하므로 슬롯을 재활용해도 새지 않는다.
        return m_mesh.Create(device,
                             data.vertices.data(), static_cast<UINT>(data.vertices.size()),
                             static_cast<UINT>(sizeof(TerrainVertex)),
                             data.indices.data(), static_cast<UINT>(data.indices.size()));
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
