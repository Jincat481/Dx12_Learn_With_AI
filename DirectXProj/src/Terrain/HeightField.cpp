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
        const float fx = std::floor(x);
        const float fz = std::floor(z);

        const int xi = static_cast<int>(fx);
        const int zi = static_cast<int>(fz);

        const float xf = x - fx;   // 셀 안에서의 위치 0~1
        const float zf = z - fz;

        // 네 모서리의 그래디언트와, 각 모서리에서 현재 위치까지의 거리 벡터를 내적한다.
        float gx = 0.0f, gz = 0.0f;

        LatticeGradient(xi,     zi,     m_params.seed, gx, gz);
        const float d00 = gx * xf + gz * zf;

        LatticeGradient(xi + 1, zi,     m_params.seed, gx, gz);
        const float d10 = gx * (xf - 1.0f) + gz * zf;

        LatticeGradient(xi,     zi + 1, m_params.seed, gx, gz);
        const float d01 = gx * xf + gz * (zf - 1.0f);

        LatticeGradient(xi + 1, zi + 1, m_params.seed, gx, gz);
        const float d11 = gx * (xf - 1.0f) + gz * (zf - 1.0f);

        const float u = Fade(xf);
        const float v = Fade(zf);

        const float value = Lerp(Lerp(d00, d10, u), Lerp(d01, d11, u), v);

        // 2D 펄린의 이론적 범위는 약 ±0.707 이다. -1~1 로 맞춰 준다.
        return value * 1.4142f;
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
        if (m_params.flatten <= 0.0f)
            return 0.0f;   // 완전 평면 (스텝 1 과 같은 결과)

        if (m_params.source == HeightSource::Image)
            return SampleImage(x, z);

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
}
