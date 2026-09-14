// =============================================================
// HeightGenCS.hlsl - 높이맵을 GPU 에서 만든다 (S71)
//
//  CPU 의 HeightField::Sample 과 **완전히 같은 식**을 옮겼다.
//   - 격자점 해시 : 같은 정수 곱셈 · xor · 시프트 (uint 는 32비트에서 넘치면 감긴다. C++ unsigned 와 같다)
//   - 펄린 : 격자점 기울기 방향 · 거리 벡터 내적 · fade 6t^5-15t^4+10t^3
//   - fBm  : 옥타브마다 주파수 × lacunarity, 진폭 × persistence, 진폭 합으로 정규화
//
//  식이 같아야 하는 이유 : 카메라가 땅에 설 때(S66)는 CPU 함수로 높이를 묻는다.
//  GPU 가 다른 지형을 만들면 보이는 땅과 서는 땅이 어긋난다.
//
//  텍셀 하나 = 스레드 하나. 512 x 512 면 26만 번의 fBm 이 동시에 돈다.
// =============================================================

cbuffer HeightGenConstants : register(b0)
{
    uint  gSeed;
    uint  gOctaves;
    uint  gNoiseType;      // 0 : 값 노이즈   1 : 펄린
    uint  gSize;           // 텍스처 한 변

    float gFrequency;
    float gAmplitude;
    float gPersistence;
    float gLacunarity;

    float gWorldWidth;
    float gWorldDepth;
    float gFlatten;
    float gPadding;
};

// 결과를 쓸 텍스처. 읽기 전용 Texture2D 가 아니라 쓰기가 되는 RWTexture2D 다.
RWTexture2D<float> gHeights : register(u0);

// ---- HeightField.cpp 의 HashLattice 와 같은 식 ----
float HashLattice(int x, int z, uint seed)
{
    uint h = seed;
    h ^= (uint)x * 374761393u;
    h ^= (uint)z * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);

    return (float(h & 0x00FFFFFFu) / float(0x00FFFFFF)) * 2.0f - 1.0f;
}

float SmoothCurve(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

float Fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float2 LatticeGradient(int x, int z, uint seed)
{
    float angle = (HashLattice(x, z, seed) + 1.0f) * 3.14159265f;
    return float2(cos(angle), sin(angle));
}

float ValueNoise(float x, float z)
{
    float fx = floor(x);
    float fz = floor(z);
    int xi = (int)fx;
    int zi = (int)fz;

    float u = SmoothCurve(x - fx);
    float v = SmoothCurve(z - fz);

    float n00 = HashLattice(xi,     zi,     gSeed);
    float n10 = HashLattice(xi + 1, zi,     gSeed);
    float n01 = HashLattice(xi,     zi + 1, gSeed);
    float n11 = HashLattice(xi + 1, zi + 1, gSeed);

    return lerp(lerp(n00, n10, u), lerp(n01, n11, u), v);
}

float PerlinNoise(float x, float z)
{
    float fx = floor(x);
    float fz = floor(z);
    int xi = (int)fx;
    int zi = (int)fz;

    float xf = x - fx;
    float zf = z - fz;

    float2 g00 = LatticeGradient(xi,     zi,     gSeed);
    float2 g10 = LatticeGradient(xi + 1, zi,     gSeed);
    float2 g01 = LatticeGradient(xi,     zi + 1, gSeed);
    float2 g11 = LatticeGradient(xi + 1, zi + 1, gSeed);

    float d00 = g00.x * xf          + g00.y * zf;
    float d10 = g10.x * (xf - 1.0f) + g10.y * zf;
    float d01 = g01.x * xf          + g01.y * (zf - 1.0f);
    float d11 = g11.x * (xf - 1.0f) + g11.y * (zf - 1.0f);

    float u = Fade(xf);
    float v = Fade(zf);

    return lerp(lerp(d00, d10, u), lerp(d01, d11, u), v) * 1.4142f;
}

float FractalNoise(float x, float z)
{
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = gFrequency;
    float normalizer = 0.0f;

    uint octaves = clamp(gOctaves, 1u, 16u);
    for (uint i = 0; i < octaves; ++i)
    {
        float n = (gNoiseType == 1u) ? PerlinNoise(x * frequency, z * frequency)
                                     : ValueNoise(x * frequency, z * frequency);
        total += n * amplitude;
        normalizer += amplitude;

        amplitude *= gPersistence;
        frequency *= gLacunarity;
    }

    return (normalizer > 0.0f) ? total / normalizer : 0.0f;
}

// 스레드 그룹 하나 = 16 x 16 스레드. CPU 는 Dispatch(ceil(size/16), ceil(size/16), 1) 로 부른다.
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    // 그룹 수를 올림으로 잡았으므로 텍스처 밖 스레드가 생긴다. 그냥 돌아간다.
    if (id.x >= gSize || id.y >= gSize)
        return;

    // CPU 의 BuildHeightTexture 와 같은 좌표 규약 : v = 0 이 +Z
    float u = id.x / float(gSize - 1);
    float v = id.y / float(gSize - 1);

    float worldX = -gWorldWidth * 0.5f + u * gWorldWidth;
    float worldZ =  gWorldDepth * 0.5f - v * gWorldDepth;

    float height = (gFlatten <= 0.0f) ? 0.0f
                                      : FractalNoise(worldX, worldZ) * gAmplitude * gFlatten;

    gHeights[id.xy] = height;
}
