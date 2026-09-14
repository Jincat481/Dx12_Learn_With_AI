#include "Core/stdafx.h"
#include "Terrain/HeightField.h"

#include <cmath>

using namespace DirectX;

namespace terrain
{
    namespace
    {
        // 격자점 하나에 -1 ~ 1 의 난수를 대응시킨다.
        //  같은 (x, z, seed) 면 항상 같은 값이 나와야 한다(해시).
        float HashLattice(int x, int z, unsigned seed)
        {
            unsigned h = seed;
            h ^= static_cast<unsigned>(x) * 374761393u;
            h ^= static_cast<unsigned>(z) * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            h ^= (h >> 16);

            // 0 ~ 1 로 정규화한 뒤 -1 ~ 1 로 옮긴다.
            return (static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFF)) * 2.0f - 1.0f;
        }

        // 부드러운 보간 계수. 선형 보간을 그대로 쓰면 격자 경계가 각지게 보인다.
        float SmoothStep(float t)
        {
            return t * t * (3.0f - 2.0f * t);
        }

        // 펄린이 쓰는 fade 곡선 : 6t^5 - 15t^4 + 10t^3
        //  smoothstep 과 달리 2차 미분까지 0 이라, 격자 경계에서 법선이 튀지 않는다.
        //  터레인처럼 기울기(법선)를 쓰는 경우에 차이가 눈에 보인다.
        float Fade(float t)
        {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        // 격자점마다 방향(단위 벡터) 하나를 정한다.
        void LatticeGradient(int x, int z, unsigned seed, float& gx, float& gz)
        {
            // HashLattice 는 -1~1 이므로 각도로 펴 준다.
            const float angle = (HashLattice(x, z, seed) + 1.0f) * 3.14159265f;
            gx = std::cos(angle);
            gz = std::sin(angle);
        }

        float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        // seed 를 직접 받는 펄린. 기후 노이즈는 지형과 다른 seed 를 써야 모양이 겹치지 않는다.
        float PerlinSeeded(float x, float z, unsigned seed)
        {
            const float fx = std::floor(x);
            const float fz = std::floor(z);

            const int xi = static_cast<int>(fx);
            const int zi = static_cast<int>(fz);

            const float xf = x - fx;
            const float zf = z - fz;

            float gx = 0.0f, gz = 0.0f;

            LatticeGradient(xi,     zi,     seed, gx, gz);
            const float d00 = gx * xf + gz * zf;

            LatticeGradient(xi + 1, zi,     seed, gx, gz);
            const float d10 = gx * (xf - 1.0f) + gz * zf;

            LatticeGradient(xi,     zi + 1, seed, gx, gz);
            const float d01 = gx * xf + gz * (zf - 1.0f);

            LatticeGradient(xi + 1, zi + 1, seed, gx, gz);
            const float d11 = gx * (xf - 1.0f) + gz * (zf - 1.0f);

            const float u = Fade(xf);
            const float v = Fade(zf);

            return Lerp(Lerp(d00, d10, u), Lerp(d01, d11, u), v) * 1.4142f;
        }

        // e0 → e1 구간에서 0 → 1. e0 > e1 이면 반대 방향으로 올라간다.
        float SmoothRange(float e0, float e1, float value)
        {
            float t = (value - e0) / (e1 - e0);
            t = (std::max)(0.0f, (std::min)(1.0f, t));
            return t * t * (3.0f - 2.0f * t);
        }
    }

    // -------------------------------------------------------------
    // 값 노이즈 : 격자점 난수를 부드럽게 보간한다.
    // -------------------------------------------------------------
    float HeightField::ValueNoise(float x, float z) const
    {
        const float fx = std::floor(x);
        const float fz = std::floor(z);

        const int xi = static_cast<int>(fx);
        const int zi = static_cast<int>(fz);

        const float u = SmoothStep(x - fx);
        const float v = SmoothStep(z - fz);

        const float n00 = HashLattice(xi,     zi,     m_params.seed);
        const float n10 = HashLattice(xi + 1, zi,     m_params.seed);
        const float n01 = HashLattice(xi,     zi + 1, m_params.seed);
        const float n11 = HashLattice(xi + 1, zi + 1, m_params.seed);

        return Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v);
    }

    // -------------------------------------------------------------
    // 펄린(그래디언트) 노이즈 (S37)
    //  격자점에 값이 아니라 "기울기 방향" 을 두고,
    //  그 점에서 현재 위치까지의 거리 벡터와 내적한다.
    //
    //  격자점 자리에서는 거리 벡터가 0 이라 값도 항상 0 이 된다.
    //  그래서 값 노이즈처럼 격자점마다 극값이 생기지 않고,
    //  능선과 골짜기가 격자축에 덜 얽매인 모양으로 나온다.
    // -------------------------------------------------------------
    float HeightField::PerlinNoise(float x, float z) const
    {
        return PerlinSeeded(x, z, m_params.seed);
    }

    float HeightField::BaseNoise(float x, float z) const
    {
        return (m_params.noiseType == NoiseType::Perlin)
             ? PerlinNoise(x, z)
             : ValueNoise(x, z);
    }

    // -------------------------------------------------------------
    // fBm : 주파수를 키우고 진폭을 줄이며 여러 층을 더한다.
    // -------------------------------------------------------------
    float HeightField::FractalNoise(float x, float z) const
    {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = m_params.frequency;
        float normalizer = 0.0f;   // 결과를 -1 ~ 1 로 되돌리기 위한 진폭 합

        const int octaves = (std::max)(1, m_params.octaves);

        for (int i = 0; i < octaves; ++i)
        {
            total += BaseNoise(x * frequency, z * frequency) * amplitude;
            normalizer += amplitude;

            amplitude *= m_params.persistence;
            frequency *= m_params.lacunarity;
        }

        return (normalizer > 0.0f) ? (total / normalizer) : 0.0f;
    }

    // -------------------------------------------------------------
    // 이미지에서 높이 읽기 (S41)
    //  격자는 원점 중심으로 놓여 있으므로 월드 좌표를 0~1 UV 로 옮긴다.
    //  이미지의 행 0 은 화면 위쪽이라 +Z 에 대응시킨다(격자 생성 순서와 맞춘다).
    // -------------------------------------------------------------
    float HeightField::SampleImage(float x, float z) const
    {
        if (!m_image || !m_image->IsValid())
            return 0.0f;

        const float width = (m_params.worldWidth > 0.0f) ? m_params.worldWidth : 1.0f;
        const float depth = (m_params.worldDepth > 0.0f) ? m_params.worldDepth : 1.0f;

        const float u = (x + width * 0.5f) / width;
        const float v = (depth * 0.5f - z) / depth;

        // 이미지 값은 0~1 이라 노이즈(-1~1)와 달리 항상 지면 위로 올라간다.
        return m_image->SampleBilinear(u, v) * m_params.amplitude * m_params.flatten;
    }

    float HeightField::Sample(float x, float z) const
    {
        // 격자는 이미 월드 높이를 담고 있다. 진폭이나 flatten 을 다시 곱하지 않는다.
        if (m_params.source == HeightSource::Grid)
            return m_grid ? m_grid->SampleBilinear(x, z) : 0.0f;

        if (m_params.flatten <= 0.0f)
            return 0.0f;   // 완전 평면 (스텝 1 과 같은 결과)

        if (m_params.source == HeightSource::Image)
            return SampleImage(x, z);

        if (m_params.biomes)
            return BiomeHeight(x, z);

        return FractalNoise(x, z) * m_params.amplitude * m_params.flatten;
    }

    // -------------------------------------------------------------
    // 법선 (S38)
    //  높이 함수 h(x, z) 로 만들어진 면의 법선은
    //      normal = normalize( -dh/dx, 1, -dh/dz )
    //  기울기는 좌우/앞뒤 한 칸씩 떨어진 높이의 차이(중앙 차분)로 근사한다.
    // -------------------------------------------------------------
    XMFLOAT3 HeightField::SampleNormal(float x, float z, float step) const
    {
        if (step <= 0.0f)
            step = 1.0f;

        const float hL = Sample(x - step, z);
        const float hR = Sample(x + step, z);
        const float hD = Sample(x, z - step);
        const float hU = Sample(x, z + step);

        const float dhdx = (hR - hL) / (2.0f * step);
        const float dhdz = (hU - hD) / (2.0f * step);

        XMVECTOR normal = XMVectorSet(-dhdx, 1.0f, -dhdz, 0.0f);
        normal = XMVector3Normalize(normal);

        XMFLOAT3 result;
        XMStoreFloat3(&result, normal);
        return result;
    }

    // -------------------------------------------------------------
    // 바이옴 (S73)
    //
    //  지형 하나의 식(fBm)만 쓰면 어디를 가도 비슷한 산이 나온다.
    //  실제 지형은 기후에 따라 모양 자체가 다르다. 사막은 평평하고 모래 언덕이 길게 늘어서고,
    //  추운 곳은 날카로운 봉우리가 솟는다.
    //
    //  1) 기후 : 아주 낮은 주파수의 노이즈 두 장 → 온도, 습도 (-1 ~ 1)
    //  2) 가중치 : 온도·습도 구간을 smoothstep 으로 나눠 네 바이옴의 비율을 정한다 (합 1)
    //  3) 높이 : 바이옴마다 다른 식으로 높이를 구하고 가중치로 섞는다
    //
    //  가중치가 연속이라 바이옴 경계에서 높이도 끊기지 않고 이어진다.
    //  0/1 로 딱 나누면 경계마다 절벽이 생긴다.
    // -------------------------------------------------------------
    void HeightField::SampleClimate(float x, float z, float& temperature, float& moisture) const
    {
        const float f = m_params.biomeFrequency;

        // 옥타브 두 개면 충분하다. 기후는 크게 변하는 값이라 잔물결이 필요 없다.
        temperature = PerlinSeeded(x * f, z * f, m_params.seed + 7919u) * 0.7f +
                      PerlinSeeded(x * f * 2.3f, z * f * 2.3f, m_params.seed + 7927u) * 0.3f;
        moisture    = PerlinSeeded(x * f, z * f, m_params.seed + 104729u) * 0.7f +
                      PerlinSeeded(x * f * 2.3f, z * f * 2.3f, m_params.seed + 104723u) * 0.3f;

        // 펄린 fBm 은 0 근처에 몰려 있다. 넓게 펴서 네 바이옴이 고르게 나오게 한다.
        temperature = (std::max)(-1.0f, (std::min)(1.0f, temperature * 1.6f));
        moisture    = (std::max)(-1.0f, (std::min)(1.0f, moisture * 1.6f));
    }

    XMFLOAT4 HeightField::SampleBiomeWeights(float x, float z) const
    {
        if (!m_params.biomes || m_params.source != HeightSource::Noise)
            return XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

        float temperature = 0.0f;
        float moisture = 0.0f;
        SampleClimate(x, z, temperature, moisture);

        const float cold = SmoothRange(-0.15f, -0.45f, temperature);   // 추울수록 1
        const float hot  = SmoothRange( 0.10f,  0.40f, temperature);   // 더울수록 1
        const float dry  = SmoothRange( 0.00f, -0.30f, moisture);
        const float wet  = SmoothRange( 0.00f,  0.30f, moisture);

        // cold 와 hot 은 온도 구간이 겹치지 않으므로 합이 1 을 넘지 않는다.
        const float tundra = cold;
        const float desert = hot * dry;
        const float forest = (1.0f - cold) * (1.0f - desert) * wet;
        const float plains = (std::max)(0.0f, 1.0f - tundra - desert - forest);

        const float total = (std::max)(1.0e-4f, tundra + desert + forest + plains);
        return XMFLOAT4(desert / total, plains / total, forest / total, tundra / total);
    }

    float HeightField::BiomeHeight(float x, float z) const
    {
        const XMFLOAT4 weights = SampleBiomeWeights(x, z);
        const float n = FractalNoise(x, z);   // -1 ~ 1

        // 사막 : 거의 평평한 바닥 위에 한 방향으로 길게 늘어선 모래 언덕
        //  z 주파수를 낮춰 늘이고, 1 - |노이즈| 로 언덕 꼭대기를 뾰족하게 만든다.
        const float dune = 1.0f - std::fabs(PerlinSeeded(x * 0.02f, z * 0.006f, m_params.seed + 31337u));
        const float desertHeight = n * 0.12f + dune * dune * 0.16f - 0.10f;

        // 초원 : 완만하게
        const float plainsHeight = n * 0.30f;

        // 숲 : 구릉
        const float forestHeight = n * 0.60f + 0.05f;

        // 설원 : 능선형(ridged) 노이즈. |n| 이 0 에 가까운 곳이 날카로운 봉우리 선이 된다.
        const float ridge = 1.0f - std::fabs(n);
        const float tundraHeight = ridge * ridge * 1.7f - 0.35f;

        const float height = desertHeight * weights.x + plainsHeight * weights.y +
                             forestHeight * weights.z + tundraHeight * weights.w;

        return height * m_params.amplitude * m_params.flatten;
    }

    // -------------------------------------------------------------
    // 렌더링된 격자 표면의 높이 (S66)
    //  Sample(x, z) 는 "함수" 의 높이다. 그런데 화면에 보이는 것은 격자 정점을 이은 삼각형이다.
    //  칸 크기가 4 인 지형에서는 둘이 꽤 차이 나서, 함수 높이에 카메라를 세우면
    //  발이 땅에 묻히거나 떠 보인다. 그래서 정점 4개를 구해 삼각형 안에서 보간한다.
    //
    //      TL ---- TR      인덱스 순서가 (TL, TR, BL) / (BL, TR, BR) 이므로
    //      |     / |       대각선은 TR 과 BL 을 잇는다.
    //      |   /   |       u + v <= 1 이면 위쪽 삼각형, 아니면 아래쪽 삼각형
    //      BL ---- BR
    // -------------------------------------------------------------
    float SampleGridSurface(const HeightField& height,
                            float originX, float originZ, float cellSize,
                            float x, float z)
    {
        if (cellSize <= 0.0f)
            return height.Sample(x, z);

        const float gridX = (x - originX) / cellSize;   // 열 방향 (+X)
        const float gridZ = (originZ - z) / cellSize;   // 행 방향 (-Z)

        const float column = std::floor(gridX);
        const float row = std::floor(gridZ);

        const float u = gridX - column;
        const float v = gridZ - row;

        const float left = originX + column * cellSize;
        const float right = left + cellSize;
        const float top = originZ - row * cellSize;
        const float bottom = top - cellSize;

        const float hTL = height.Sample(left, top);
        const float hTR = height.Sample(right, top);
        const float hBL = height.Sample(left, bottom);
        const float hBR = height.Sample(right, bottom);

        if (u + v <= 1.0f)
            return hTL + (hTR - hTL) * u + (hBL - hTL) * v;                 // 삼각형 (TL, TR, BL)

        return hBR + (hBL - hBR) * (1.0f - u) + (hTR - hBR) * (1.0f - v);   // 삼각형 (BL, TR, BR)
    }
}
