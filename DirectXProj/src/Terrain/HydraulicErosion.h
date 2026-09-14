#pragma once
#include "Core/stdafx.h"

#include <climits>
#include <random>

// =============================================================
// HydraulicErosion (S74)
//  물방울 입자 하나하나를 흘려보내 지형을 깎고 쌓는다.
//
//  노이즈 지형은 "산이 어떻게 생겼는가" 만 흉내 낸다. 실제 산의 모양은
//  빗물이 수만 년 동안 깎아 만든 것이라 계곡이 한데 모이고 능선이 날카롭다.
//
//  물방울 하나의 일생
//   1) 격자 위 임의의 점에 떨어진다 (물 1, 속도 1, 실은 흙 0)
//   2) 경사를 따라 한 칸씩 흘러내린다 (관성으로 이전 방향을 조금 유지)
//   3) 흙을 실을 수 있는 양(capacity) = 내리막 기울기 × 속도 × 남은 물
//        실은 흙 < 용량 → 발밑을 깎아 싣는다 (주변 반경에 나눠 깎아 구덩이를 막는다)
//        실은 흙 > 용량 또는 오르막 → 발밑에 내려놓는다 (계곡 바닥이 메워진다)
//   4) 내리막이면 빨라지고, 물은 조금씩 증발한다. 수명이 다하면 끝
//
//  이것을 수만 번 반복하면 물길이 겹치는 곳이 계곡이 되고, 흙이 쌓인 곳이 평지가 된다.
// =============================================================
namespace terrain
{
    class HeightGrid;

    struct ErosionParams
    {
        float inertia = 0.05f;         // 0 이면 경사만 따르고, 1 이면 처음 방향으로만 간다
        float capacity = 4.0f;         // 흙을 실어 나를 수 있는 양의 배율
        float minCapacity = 0.01f;     // 거의 평평해도 조금은 실어 간다
        float erodeSpeed = 0.3f;       // 용량이 남을 때 그 중 얼마를 깎을지
        float depositSpeed = 0.3f;     // 넘칠 때 그 중 얼마를 내려놓을지
        float evaporateSpeed = 0.01f;  // 한 걸음마다 증발하는 물의 비율
        float gravity = 4.0f;
        int   maxLifetime = 30;        // 한 물방울이 움직이는 최대 걸음 수
        int   radius = 3;              // 깎을 때 퍼뜨리는 반경 (격자 칸)
        float heightScale = 40.0f;     // 월드 높이를 이 값으로 나눠 계산한다 (위 계수들이 0~1 높이 기준이라서)
    };

    // 이번에 바뀐 격자 범위. 지형은 이 범위와 겹치는 청크만 다시 만든다.
    struct GridBounds
    {
        int minColumn = INT_MAX;
        int minRow = INT_MAX;
        int maxColumn = INT_MIN;
        int maxRow = INT_MIN;

        void Add(int column, int row)
        {
            minColumn = (std::min)(minColumn, column);
            minRow = (std::min)(minRow, row);
            maxColumn = (std::max)(maxColumn, column);
            maxRow = (std::max)(maxRow, row);
        }

        bool IsEmpty() const { return minColumn > maxColumn || minRow > maxRow; }
    };

    class HydraulicErosion
    {
    public:
        void SetParams(const ErosionParams& params) { m_params = params; m_brushRadius = -1; }
        const ErosionParams& GetParams() const { return m_params; }

        // 격자 전체에 무작위로 물방울을 떨어뜨린다.
        void Simulate(HeightGrid& grid, int droplets, std::mt19937& rng, GridBounds& touched);

        // 원 안에만 떨어뜨린다 (브러시). 중심과 반경은 격자 좌표.
        void SimulateInCircle(HeightGrid& grid, float centerColumn, float centerRow, float radius,
                              int droplets, std::mt19937& rng, GridBounds& touched);

    private:
        struct BrushOffset
        {
            int   dx;
            int   dy;
            float weight;
        };

        void EnsureBrush();
        void RunDroplet(HeightGrid& grid, float positionX, float positionY, GridBounds& touched);

        ErosionParams m_params;
        std::vector<BrushOffset> m_brush;
        int m_brushRadius = -1;
    };
}
