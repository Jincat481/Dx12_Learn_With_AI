#pragma once
#include "Core/stdafx.h"

// =============================================================
// HeightGrid (S69)
//  편집할 수 있는 높이 표본 격자.
//
//  노이즈는 "함수" 라서 한 곳만 골라 올릴 수 없다. 식을 바꾸면 지형 전체가 바뀐다.
//  편집하려면 높이를 "값" 으로 들고 있어야 한다. 그래서 노이즈를 한 번 구워(bake)
//  격자에 담고, 그 뒤로는 격자 값을 직접 고친다.
//  스텝 3 의 이미지 하이트맵과 같은 "표본" 이지만 8비트가 아니라 float 라 계단이 없다.
//
//  격자점 (c, r) 의 월드 위치 = (originX + c * cellSize, originZ - r * cellSize)
//  지형 정점과 같은 자리에 두면 정점 높이가 보간 없이 그대로 나온다.
// =============================================================
namespace terrain
{
    class HeightField;

    class HeightGrid
    {
    public:
        void Resize(int columns, int rows, float originX, float originZ, float cellSize);

        // 높이 함수를 격자점마다 한 번씩 샘플링해 담는다.
        void BakeFrom(const HeightField& source);

        bool IsValid() const { return m_columns > 1 && m_rows > 1 && !m_heights.empty(); }

        int   GetColumns()  const { return m_columns; }
        int   GetRows()     const { return m_rows; }
        float GetCellSize() const { return m_cellSize; }
        float GetOriginX()  const { return m_originX; }
        float GetOriginZ()  const { return m_originZ; }

        // 월드 좌표 → 격자 좌표(실수). 행은 -Z 방향으로 커진다.
        float ToColumn(float x) const { return (x - m_originX) / m_cellSize; }
        float ToRow(float z)    const { return (m_originZ - z) / m_cellSize; }

        float ColumnToX(int column) const { return m_originX + column * m_cellSize; }
        float RowToZ(int row)       const { return m_originZ - row * m_cellSize; }

        float Get(int column, int row) const;           // 범위 밖은 가장자리 값
        void  Set(int column, int row, float height);   // 범위 밖은 무시

        // 격자점 사이는 이중선형으로 보간한다. 범위 밖은 가장자리 값.
        float SampleBilinear(float x, float z) const;

        std::vector<float>&       Data()       { return m_heights; }
        const std::vector<float>& Data() const { return m_heights; }

        // 큰 데이터는 JSON 에 넣지 않고 옆에 이진 파일로 둔다. (S70)
        bool SaveRaw(const std::wstring& path) const;
        bool LoadRaw(const std::wstring& path);

    private:
        std::vector<float> m_heights;   // 행 우선 : index = row * columns + column
        int   m_columns = 0;
        int   m_rows = 0;
        float m_originX = 0.0f;
        float m_originZ = 0.0f;
        float m_cellSize = 1.0f;
    };
}
