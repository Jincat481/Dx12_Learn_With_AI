#pragma once
#include "Core/stdafx.h"

// =============================================================
// HeightField (S36, S37)
//  월드 좌표 (x, z) 를 넣으면 높이 y 를 돌려주는 "높이 함수".
//
//  이미지 하이트맵 대신 절차적 노이즈로 만든다.
//   - 에셋 파일이 없어도 항상 같은 지형이 나온다(seed 로 결정)
//   - 해상도 제한이 없다. 격자를 촘촘히 해도 계단이 생기지 않는다
//
//  fBm(fractal Brownian motion)
//   여러 옥타브의 노이즈를 겹친다. 옥타브가 올라갈수록
//   주파수는 lacunarity 배로 촘촘해지고 진폭은 persistence 배로 작아진다.
//   → 큰 산맥 위에 작은 굴곡이 얹히는 자연스러운 모양이 나온다.
// =============================================================
namespace terrain
{
    struct HeightParams
    {
        unsigned seed = 1337;
        float frequency = 0.012f;    // 낮을수록 지형이 완만하고 넓다
        float amplitude = 14.0f;     // 최대 높이(월드 단위)
        int   octaves = 5;           // 겹치는 노이즈 층 수
        float persistence = 0.5f;    // 옥타브마다 진폭에 곱하는 값
        float lacunarity = 2.0f;     // 옥타브마다 주파수에 곱하는 값
        float flatten = 1.0f;        // 0 이면 완전 평면(스텝 1 과 같은 결과)
    };

    class HeightField
    {
    public:
        HeightField() = default;
        explicit HeightField(const HeightParams& params) : m_params(params) {}

        void SetParams(const HeightParams& params) { m_params = params; }
        const HeightParams& GetParams() const { return m_params; }

        // 월드 좌표에서의 높이
        float Sample(float x, float z) const;

        // 중앙 차분으로 법선을 구한다. (S38)
        //  기울기 (dh/dx, dh/dz) 를 알면 법선은 (-dh/dx, 1, -dh/dz) 를 정규화한 것이다.
        DirectX::XMFLOAT3 SampleNormal(float x, float z, float step) const;

    private:
        float ValueNoise(float x, float z) const;
        float FractalNoise(float x, float z) const;

        HeightParams m_params;
    };
}
