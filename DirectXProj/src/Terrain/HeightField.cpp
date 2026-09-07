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
            total += ValueNoise(x * frequency, z * frequency) * amplitude;
            normalizer += amplitude;

            amplitude *= m_params.persistence;
            frequency *= m_params.lacunarity;
        }

        return (normalizer > 0.0f) ? (total / normalizer) : 0.0f;
    }

    float HeightField::Sample(float x, float z) const
    {
        if (m_params.flatten <= 0.0f)
            return 0.0f;   // 완전 평면 (스텝 1 과 같은 결과)

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
