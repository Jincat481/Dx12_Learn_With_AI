#include "Core/stdafx.h"
#include "Terrain/HydraulicErosion.h"
#include "Terrain/HeightGrid.h"

#include <cmath>

namespace terrain
{
    namespace
    {
        // 격자 좌표 (x, y) 에서의 높이와 기울기. 네 격자점을 이중선형으로 보간한다.
        //  기울기 = 높이를 x, y 로 미분한 값. 이 반대 방향이 물이 흐르는 방향이다.
        float HeightAndGradient(const std::vector<float>& heights, int columns,
                                float x, float y, float invScale,
                                float& outGradientX, float& outGradientY)
        {
            const int nodeX = static_cast<int>(x);
            const int nodeY = static_cast<int>(y);
            const float u = x - nodeX;
            const float v = y - nodeY;

            const size_t index = static_cast<size_t>(nodeY) * columns + nodeX;
            const float nw = heights[index] * invScale;
            const float ne = heights[index + 1] * invScale;
            const float sw = heights[index + columns] * invScale;
            const float se = heights[index + columns + 1] * invScale;

            outGradientX = (ne - nw) * (1.0f - v) + (se - sw) * v;
            outGradientY = (sw - nw) * (1.0f - u) + (se - ne) * u;

            return nw * (1.0f - u) * (1.0f - v) + ne * u * (1.0f - v) +
                   sw * (1.0f - u) * v          + se * u * v;
        }
    }

    // -------------------------------------------------------------
    // 깎을 때 쓰는 원형 가중치
    //  발밑 한 점만 깎으면 좁고 깊은 구덩이가 파여 물방울이 거기 갇힌다.
    //  반경 안에 거리가 가까울수록 많이 나눠 깎는다. 가중치 합은 1.
    // -------------------------------------------------------------
    void HydraulicErosion::EnsureBrush()
    {
        if (m_brushRadius == m_params.radius)
            return;

        m_brush.clear();
        m_brushRadius = (std::max)(1, m_params.radius);

        float total = 0.0f;
        for (int dy = -m_brushRadius; dy <= m_brushRadius; ++dy)
        {
            for (int dx = -m_brushRadius; dx <= m_brushRadius; ++dx)
            {
                const float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                const float weight = static_cast<float>(m_brushRadius) - distance;
                if (weight <= 0.0f)
                    continue;

                m_brush.push_back({ dx, dy, weight });
                total += weight;
            }
        }

        for (BrushOffset& offset : m_brush)
            offset.weight /= total;
    }

    void HydraulicErosion::Simulate(HeightGrid& grid, int droplets, std::mt19937& rng, GridBounds& touched)
    {
        if (!grid.IsValid())
            return;

        EnsureBrush();

        // 오른쪽/아래 이웃이 있어야 보간할 수 있으므로 마지막 줄 바로 앞까지만 떨어뜨린다.
        std::uniform_real_distribution<float> randomX(0.0f, static_cast<float>(grid.GetColumns() - 1) - 0.001f);
        std::uniform_real_distribution<float> randomY(0.0f, static_cast<float>(grid.GetRows() - 1) - 0.001f);

        for (int i = 0; i < droplets; ++i)
            RunDroplet(grid, randomX(rng), randomY(rng), touched);
    }

    void HydraulicErosion::SimulateInCircle(HeightGrid& grid, float centerColumn, float centerRow, float radius,
                                            int droplets, std::mt19937& rng, GridBounds& touched)
    {
        if (!grid.IsValid() || radius <= 0.0f)
            return;

        EnsureBrush();

        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        const float maxX = static_cast<float>(grid.GetColumns() - 1) - 0.001f;
        const float maxY = static_cast<float>(grid.GetRows() - 1) - 0.001f;

        for (int i = 0; i < droplets; ++i)
        {
            // 원 안에 고르게 : 반지름에 sqrt 를 씌우지 않으면 가운데에 몰린다.
            const float r = radius * std::sqrt(unit(rng));
            const float angle = unit(rng) * 6.2831853f;

            const float x = centerColumn + std::cos(angle) * r;
            const float y = centerRow + std::sin(angle) * r;
            if (x < 0.0f || y < 0.0f || x >= maxX || y >= maxY)
                continue;

            RunDroplet(grid, x, y, touched);
        }
    }

    // -------------------------------------------------------------
    // 물방울 하나
    // -------------------------------------------------------------
    void HydraulicErosion::RunDroplet(HeightGrid& grid, float positionX, float positionY, GridBounds& touched)
    {
        const int columns = grid.GetColumns();
        const int rows = grid.GetRows();
        std::vector<float>& heights = grid.Data();

        const float scale = (std::max)(0.001f, m_params.heightScale);
        const float invScale = 1.0f / scale;

        float directionX = 0.0f;
        float directionY = 0.0f;
        float speed = 1.0f;
        float water = 1.0f;
        float sediment = 0.0f;

        for (int step = 0; step < m_params.maxLifetime; ++step)
        {
            const int nodeX = static_cast<int>(positionX);
            const int nodeY = static_cast<int>(positionY);
            const size_t nodeIndex = static_cast<size_t>(nodeY) * columns + nodeX;
            const float cellX = positionX - nodeX;
            const float cellY = positionY - nodeY;

            float gradientX = 0.0f;
            float gradientY = 0.0f;
            const float height = HeightAndGradient(heights, columns, positionX, positionY, invScale, gradientX, gradientY);

            // 관성 : 이전 방향을 조금 유지하고 나머지는 내리막(기울기 반대)을 따른다.
            directionX = directionX * m_params.inertia - gradientX * (1.0f - m_params.inertia);
            directionY = directionY * m_params.inertia - gradientY * (1.0f - m_params.inertia);

            const float length = std::sqrt(directionX * directionX + directionY * directionY);
            if (length < 1.0e-6f)
                break;   // 완전히 평평한 웅덩이 : 더 갈 곳이 없다

            directionX /= length;
            directionY /= length;

            // 한 칸씩 움직인다. 기울기 크기와 무관하게 걸음 폭을 같게 해야 평지에서도 흘러간다.
            positionX += directionX;
            positionY += directionY;

            if (positionX < 0.0f || positionY < 0.0f ||
                positionX >= static_cast<float>(columns - 1) || positionY >= static_cast<float>(rows - 1))
                break;   // 지형 밖으로 흘러 나갔다

            float unusedX = 0.0f;
            float unusedY = 0.0f;
            const float newHeight = HeightAndGradient(heights, columns, positionX, positionY, invScale, unusedX, unusedY);
            const float deltaHeight = newHeight - height;   // 내리막이면 음수

            // 안전장치 : 어디선가 값이 망가졌다면 이 물방울은 버린다. NaN 은 격자 전체로 번진다.
            if (!std::isfinite(deltaHeight) || !std::isfinite(sediment))
                break;

            // 실을 수 있는 흙의 양 : 가파르게 내려갈수록, 빠를수록, 물이 많을수록 크다.
            const float capacity = (std::max)(-deltaHeight * speed * water * m_params.capacity, m_params.minCapacity);

            if (sediment > capacity || deltaHeight > 0.0f)
            {
                // ---- 쌓기 ----
                //  오르막이면 그 턱을 메울 만큼만, 넘치면 넘친 양의 일부를 내려놓는다.
                const float deposit = (deltaHeight > 0.0f)
                    ? (std::min)(deltaHeight, sediment)
                    : (sediment - capacity) * m_params.depositSpeed;

                sediment -= deposit;

                // 떠난 자리(이전 칸)의 네 격자점에 이중선형 비율로 나눠 놓는다.
                const float amount = deposit * scale;
                heights[nodeIndex]               += amount * (1.0f - cellX) * (1.0f - cellY);
                heights[nodeIndex + 1]           += amount * cellX * (1.0f - cellY);
                heights[nodeIndex + columns]     += amount * (1.0f - cellX) * cellY;
                heights[nodeIndex + columns + 1] += amount * cellX * cellY;

                touched.Add(nodeX, nodeY);
                touched.Add(nodeX + 1, nodeY + 1);
            }
            else
            {
                // ---- 깎기 ----
                //  남은 용량만큼 깎되, 내려온 높이보다 더 깎지는 않는다.
                const float erode = (std::min)((capacity - sediment) * m_params.erodeSpeed, -deltaHeight);

                for (const BrushOffset& offset : m_brush)
                {
                    const int x = nodeX + offset.dx;
                    const int y = nodeY + offset.dy;
                    if (x < 0 || y < 0 || x >= columns || y >= rows)
                        continue;

                    // 물이 흘러갈 자리(newHeight)보다 낮게는 깎지 않는다.
                    //  이 제한이 없으면 반경 안의 더 낮은 격자점까지 깎여 물방울이 모이는 곳마다 구덩이가 파이고,
                    //  구덩이가 물을 더 모아 더 깊어지는 되먹임으로 높이가 -무한대로 발산한다. (S75)
                    const size_t index = static_cast<size_t>(y) * columns + x;
                    const float room = (std::max)(0.0f, heights[index] * invScale - newHeight);
                    const float removed = (std::min)(erode * offset.weight, room);

                    heights[index] -= removed * scale;
                    sediment += removed;
                }

                touched.Add(nodeX - m_brushRadius, nodeY - m_brushRadius);
                touched.Add(nodeX + m_brushRadius, nodeY + m_brushRadius);
            }

            // 내리막(음수 deltaHeight)이면 빨라진다 : v² += -Δh · g
            speed = std::sqrt((std::max)(0.0f, speed * speed - deltaHeight * m_params.gravity));
            water *= (1.0f - m_params.evaporateSpeed);
        }
    }
}
