#pragma once
#include "Core/stdafx.h"
#include "Graphics/Mesh.h"
#include "Graphics/Vertex.h"
#include "Terrain/HeightField.h"

// =============================================================
// TerrainChunk (S49)
//  지형을 한 덩어리로 만들면 화면 밖에 있어도 통째로 그려야 한다.
//  타일로 쪼개 두면 "보이는 타일만" 그릴 수 있다.
//
//  청크 하나는
//   - 자기 영역의 격자 메시 (LOD 단계별로 여러 벌)
//   - 자기 영역을 감싸는 AABB (컬링 판정용)
//  를 갖는다.
//
//  LOD (S51)
//   같은 정점 격자에서 간격(stride)만 벌려 인덱스를 다시 만든다.
//   LOD 0 = 한 칸씩, LOD 1 = 두 칸씩, LOD 2 = 네 칸씩 ...
//   정점 버퍼는 한 벌만 두고 인덱스 버퍼만 여러 벌 두면 메모리가 절약된다.
// =============================================================
namespace terrain
{
    class TerrainChunk
    {
    public:
        static constexpr int kMaxLod = 4;

        // originX/Z : 청크 왼쪽 위 모서리의 월드 좌표
        // cells     : 청크 한 변의 칸 수 (2의 거듭제곱이어야 LOD 를 끝까지 내릴 수 있다)
        void SetSkirtEnabled(bool enabled) { m_skirtEnabled = enabled; }

        bool Build(ID3D11Device* device,
                   float originX, float originZ,
                   int cells, float cellSize,
                   const HeightField* height);

        void Release();

        bool IsValid() const { return m_mesh.IsValid(); }

        const Mesh& GetMesh() const { return m_mesh; }

        // LOD 단계별 인덱스 범위. 정점 버퍼는 공유한다.
        UINT GetIndexOffset(int lod) const;
        UINT GetIndexCount(int lod) const;
        int  GetLodCount() const { return m_lodCount; }

        const DirectX::XMFLOAT3& GetCenter()  const { return m_center; }
        const DirectX::XMFLOAT3& GetExtents() const { return m_extents; }

        float GetMinHeight() const { return m_minHeight; }
        float GetMaxHeight() const { return m_maxHeight; }

    private:
        Mesh m_mesh;

        // LOD 별 인덱스 구간 (하나의 인덱스 버퍼에 이어 붙인다)
        UINT m_indexOffset[kMaxLod] = {};
        UINT m_indexCount[kMaxLod] = {};
        int  m_lodCount = 1;
        bool m_skirtEnabled = true;

        DirectX::XMFLOAT3 m_center{};
        DirectX::XMFLOAT3 m_extents{};
        float m_minHeight = 0.0f;
        float m_maxHeight = 0.0f;
    };
}
