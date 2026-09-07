#include "Core/stdafx.h"
#include "Terrain/TerrainMeshBuilder.h"

using namespace DirectX;

namespace terrain
{
    bool BuildGrid(const GridDesc& desc, MeshData& out, const HeightField* height)
    {
        if (desc.cellsX <= 0 || desc.cellsZ <= 0 || desc.cellSize <= 0.0f)
        {
            dxutil::DebugLog(L"[Terrain] 격자 설정이 잘못되었다 (cellsX=%d, cellsZ=%d, cellSize=%.3f)",
                             desc.cellsX, desc.cellsZ, desc.cellSize);
            return false;
        }

        const int cols = desc.cellsX + 1;    // 한 행의 정점 수
        const int rows = desc.cellsZ + 1;    // 행의 수

        const float width = desc.GetWidth();
        const float depth = desc.GetDepth();
        const float halfWidth = width * 0.5f;
        const float halfDepth = depth * 0.5f;

        // ---- 정점 ----
        out.vertices.clear();
        out.vertices.reserve(static_cast<size_t>(rows) * cols);

        const float du = desc.uvTiling / static_cast<float>(desc.cellsX);
        const float dv = desc.uvTiling / static_cast<float>(desc.cellsZ);

        for (int r = 0; r < rows; ++r)
        {
            // r 이 커질수록 -Z 로 내려간다. (인덱스 winding 을 시계 방향으로 맞추기 위한 순서)
            const float z = halfDepth - r * desc.cellSize;

            for (int c = 0; c < cols; ++c)
            {
                const float x = -halfWidth + c * desc.cellSize;

                TerrainVertex vertex;

                if (height)
                {
                    // 스텝 2 : 높이 함수에서 y 를 받고, 기울기로 법선을 만든다.
                    vertex.position = XMFLOAT3(x, height->Sample(x, z), z);
                    vertex.normal   = height->SampleNormal(x, z, desc.cellSize);
                }
                else
                {
                    // 스텝 1 : 평면이므로 높이 0, 법선은 위쪽
                    vertex.position = XMFLOAT3(x, 0.0f, z);
                    vertex.normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
                }

                vertex.uv = XMFLOAT2(c * du, r * dv);

                out.vertices.push_back(vertex);
            }
        }

        // ---- 인덱스 ----
        //  칸 하나 = 삼각형 2개.
        //      topLeft(r,c) --- topRight(r,c+1)
        //           |        \        |
        //   bottomLeft(r+1,c) --- bottomRight(r+1,c+1)
        //
        //  왼손 좌표계에서 위(+Y)에서 내려다볼 때 시계 방향이 앞면이다.
        out.indices.clear();
        out.indices.reserve(static_cast<size_t>(desc.cellsX) * desc.cellsZ * 6);

        for (int r = 0; r < desc.cellsZ; ++r)
        {
            for (int c = 0; c < desc.cellsX; ++c)
            {
                const uint32_t topLeft     = static_cast<uint32_t>(r * cols + c);
                const uint32_t topRight    = topLeft + 1;
                const uint32_t bottomLeft  = static_cast<uint32_t>((r + 1) * cols + c);
                const uint32_t bottomRight = bottomLeft + 1;

                out.indices.push_back(topLeft);
                out.indices.push_back(topRight);
                out.indices.push_back(bottomLeft);

                out.indices.push_back(bottomLeft);
                out.indices.push_back(topRight);
                out.indices.push_back(bottomRight);
            }
        }

        const HeightRange range = GetHeightRange(out);
        dxutil::DebugLog(L"[Terrain] 높이 범위 : %.2f ~ %.2f", range.minY, range.maxY);

        dxutil::DebugLog(L"[Terrain] 격자 생성 : %d x %d 칸, 정점 %zu개, 삼각형 %zu개",
                         desc.cellsX, desc.cellsZ,
                         out.vertices.size(), out.indices.size() / 3);
        return true;
    }

    HeightRange GetHeightRange(const MeshData& data)
    {
        HeightRange range;
        if (data.vertices.empty())
            return range;

        range.minY = data.vertices.front().position.y;
        range.maxY = range.minY;

        for (const TerrainVertex& vertex : data.vertices)
        {
            range.minY = (std::min)(range.minY, vertex.position.y);
            range.maxY = (std::max)(range.maxY, vertex.position.y);
        }
        return range;
    }
}
