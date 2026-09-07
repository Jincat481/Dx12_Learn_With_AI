#pragma once
#include "Core/stdafx.h"
#include "Terrain/HeightMapImage.h"

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
    // 어떤 기본 노이즈를 쓸지. (S37)
    //  Value  : 격자점에 "값"을 두고 보간한다. 구현이 쉽지만 덩어리져 보이고
    //           격자축을 따라 무늬가 도드라진다.
    //  Perlin : 격자점에 "기울기(그래디언트)"를 두고 거리 벡터와 내적한다.
    //           격자점에서 값이 항상 0이라 규칙적인 얼룩이 덜 생기고 능선이 자연스럽다.
    enum class NoiseType
    {
        Value,
        Perlin,
    };

    // 높이를 어디서 가져올지. (S41)
    //  Noise : 함수. 아무 좌표나 물어봐도 값이 나온다. 해상도 제한이 없다.
    //  Image : 표본. 픽셀 사이는 보간해야 하고 8비트라 256단계로 양자화되어 있다.
    enum class HeightSource
    {
        Noise,
        Image,
    };

    struct HeightParams
    {
        HeightSource source = HeightSource::Noise;
        NoiseType noiseType = NoiseType::Perlin;
        unsigned seed = 1337;
        float frequency = 0.012f;    // 낮을수록 지형이 완만하고 넓다
        float amplitude = 14.0f;     // 최대 높이(월드 단위)
        int   octaves = 5;           // 겹치는 노이즈 층 수
        float persistence = 0.5f;    // 옥타브마다 진폭에 곱하는 값
        float lacunarity = 2.0f;     // 옥타브마다 주파수에 곱하는 값
        float flatten = 1.0f;        // 0 이면 완전 평면(스텝 1 과 같은 결과)

        // 이미지 모드에서 월드 좌표를 UV 로 바꿀 때 쓰는 지형 크기
        float worldWidth = 128.0f;
        float worldDepth = 128.0f;
    };

    class HeightField
    {
    public:
        HeightField() = default;
        explicit HeightField(const HeightParams& params) : m_params(params) {}

        void SetParams(const HeightParams& params) { m_params = params; }

        // 이미지 모드에서 쓸 높이맵. 여러 터레인이 한 장을 공유할 수 있다.
        void SetImage(std::shared_ptr<HeightMapImage> image) { m_image = std::move(image); }
        const std::shared_ptr<HeightMapImage>& GetImage() const { return m_image; }
        const HeightParams& GetParams() const { return m_params; }

        // 월드 좌표에서의 높이
        float Sample(float x, float z) const;

        // 중앙 차분으로 법선을 구한다. (S38)
        //  기울기 (dh/dx, dh/dz) 를 알면 법선은 (-dh/dx, 1, -dh/dz) 를 정규화한 것이다.
        DirectX::XMFLOAT3 SampleNormal(float x, float z, float step) const;

    private:
        float BaseNoise(float x, float z) const;    // 설정에 따라 아래 둘 중 하나를 부른다
        float ValueNoise(float x, float z) const;
        float PerlinNoise(float x, float z) const;
        float FractalNoise(float x, float z) const;

        float SampleImage(float x, float z) const;

        HeightParams m_params;
        std::shared_ptr<HeightMapImage> m_image;
    };
}
