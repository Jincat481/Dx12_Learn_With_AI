#include "Core/stdafx.h"
#include "Terrain/HeightGrid.h"
#include "Terrain/HeightField.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace terrain
{
    namespace
    {
        // 이진 파일 머리말. 읽을 때 격자 크기가 맞는지 확인하는 데 쓴다.
        struct RawHeader
        {
            char    magic[4];     // "HGT1"
            int32_t columns;
            int32_t rows;
            float   originX;
            float   originZ;
            float   cellSize;
        };
    }

    void HeightGrid::Resize(int columns, int rows, float originX, float originZ, float cellSize)
    {
        m_columns = (std::max)(2, columns);
        m_rows = (std::max)(2, rows);
        m_originX = originX;
        m_originZ = originZ;
        m_cellSize = (cellSize > 0.0f) ? cellSize : 1.0f;

        m_heights.assign(static_cast<size_t>(m_columns) * m_rows, 0.0f);
    }

    void HeightGrid::BakeFrom(const HeightField& source)
    {
        for (int row = 0; row < m_rows; ++row)
        {
            const float z = RowToZ(row);
            for (int column = 0; column < m_columns; ++column)
                m_heights[static_cast<size_t>(row) * m_columns + column] = source.Sample(ColumnToX(column), z);
        }
    }

    float HeightGrid::Get(int column, int row) const
    {
        if (!IsValid())
            return 0.0f;

        column = (std::max)(0, (std::min)(m_columns - 1, column));
        row = (std::max)(0, (std::min)(m_rows - 1, row));
        return m_heights[static_cast<size_t>(row) * m_columns + column];
    }

    void HeightGrid::Set(int column, int row, float height)
    {
        if (column < 0 || column >= m_columns || row < 0 || row >= m_rows)
            return;

        m_heights[static_cast<size_t>(row) * m_columns + column] = height;
    }

    float HeightGrid::SampleBilinear(float x, float z) const
    {
        if (!IsValid())
            return 0.0f;

        const float gridX = (std::max)(0.0f, (std::min)(static_cast<float>(m_columns - 1), ToColumn(x)));
        const float gridZ = (std::max)(0.0f, (std::min)(static_cast<float>(m_rows - 1), ToRow(z)));

        const int column = (std::min)(m_columns - 2, static_cast<int>(gridX));
        const int row = (std::min)(m_rows - 2, static_cast<int>(gridZ));

        const float tx = gridX - column;
        const float tz = gridZ - row;

        const float h00 = Get(column, row);
        const float h10 = Get(column + 1, row);
        const float h01 = Get(column, row + 1);
        const float h11 = Get(column + 1, row + 1);

        const float top = h00 + (h10 - h00) * tx;
        const float bottom = h01 + (h11 - h01) * tx;
        return top + (bottom - top) * tz;
    }

    // -------------------------------------------------------------
    // 이진 저장 (S70)
    //  257 x 257 격자는 float 66,049 개다. JSON 에 숫자로 늘어놓으면
    //  수 MB 짜리 텍스트가 되고 파싱도 느리다. 메모리를 그대로 쓰면 250KB 남짓이다.
    // -------------------------------------------------------------
    bool HeightGrid::SaveRaw(const std::wstring& path) const
    {
        if (!IsValid())
            return false;

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

        std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
        if (!file)
            return false;

        RawHeader header{};
        std::memcpy(header.magic, "HGT1", 4);
        header.columns = m_columns;
        header.rows = m_rows;
        header.originX = m_originX;
        header.originZ = m_originZ;
        header.cellSize = m_cellSize;

        file.write(static_cast<const char*>(static_cast<void*>(&header)), sizeof(header));
        file.write(static_cast<const char*>(static_cast<void*>(const_cast<float*>(m_heights.data()))),
                   static_cast<std::streamsize>(m_heights.size() * sizeof(float)));

        return static_cast<bool>(file);
    }

    bool HeightGrid::LoadRaw(const std::wstring& path)
    {
        std::ifstream file(std::filesystem::path(path), std::ios::binary);
        if (!file)
            return false;

        RawHeader header{};
        file.read(static_cast<char*>(static_cast<void*>(&header)), sizeof(header));

        if (!file || std::memcmp(header.magic, "HGT1", 4) != 0 ||
            header.columns < 2 || header.rows < 2 || header.cellSize <= 0.0f)
            return false;

        std::vector<float> heights(static_cast<size_t>(header.columns) * header.rows);
        file.read(static_cast<char*>(static_cast<void*>(heights.data())),
                  static_cast<std::streamsize>(heights.size() * sizeof(float)));
        if (!file)
            return false;

        m_columns = header.columns;
        m_rows = header.rows;
        m_originX = header.originX;
        m_originZ = header.originZ;
        m_cellSize = header.cellSize;
        m_heights = std::move(heights);
        return true;
    }
}
