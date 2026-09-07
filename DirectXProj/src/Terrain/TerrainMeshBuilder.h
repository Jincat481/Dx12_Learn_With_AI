#pragma once
#include "Core/stdafx.h"
#include "Graphics/Vertex.h"

// =============================================================
// TerrainMeshBuilder (S30)
//  XZ 평면에 놓인 평평한 격자(그리드) 메시를 만든다. Y 는 높이 축이다.
//
//  칸(cell) 이 cellsX × cellsZ 이면 정점은 (cellsX+1) × (cellsZ+1) 개다.
//  칸 하나는 삼각형 2개 = 인덱스 6개.
//
//      (0,0) ---- (1,0) ---- (2,0)      행(row) r 은 +Z 에서 -Z 로 내려간다.
//        |   \      |   \      |        열(col) c 는 -X 에서 +X 로 간다.
//      (0,1) ---- (1,1) ---- (2,1)
//
//  정점 인덱스 = r * (cellsX + 1) + c   ← 2차원 격자를 1차원 배열에 담는 공식
//
//  다음 스텝(하이트맵)에서 position.y 와 normal 만 바꾸면 되도록
//  지금부터 NORMAL 과 TEXCOORD 를 함께 채워 둔다.
// =============================================================
namespace terrain
{
    struct GridDesc
    {
        int   cellsX = 64;        // X 방향 칸 수
        int   cellsZ = 64;        // Z 방향 칸 수
        float cellSize = 1.0f;    // 칸 하나의 한 변 길이(월드 단위)
        float uvTiling = 1.0f;    // UV 반복 횟수 (1 이면 전체가 0~1)

        float GetWidth() const { return cellsX * cellSize; }
        float GetDepth() const { return cellsZ * cellSize; }
    };

    struct MeshData
    {
        std::vector<TerrainVertex> vertices;
        std::vector<uint32_t>      indices;
    };

    // 격자 메시를 만든다. 원점이 격자의 중심이 되도록 배치한다.
    bool BuildGrid(const GridDesc& desc, MeshData& out);
}
