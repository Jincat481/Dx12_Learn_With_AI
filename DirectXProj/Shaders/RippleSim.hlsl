// =============================================================
// RippleSim.hlsl - 파동 입자를 높이 텍스처에 그리기 (S79, S89)
//
//  S79 에서는 이 파일이 격자 파동 방정식을 풀었다. 선형 방정식이라 마주 오는 물결이 서로 통과했다.
//  S89 부터 물결은 CPU 의 파동 입자(WaveParticles)가 들고 있고, 이 셰이더는 입자를 그리기만 한다.
//
//  입자 하나 = 진행 방향으로 놓인 작은 사각형
//   s : 진행 방향 좌표 (반경 단위, 앞 +1 ~ 뒤 -3)
//   t : 옆 방향 좌표   (반경 단위, -1 ~ +1)
//
//     높이 = 세기 · 옆 창(t) · 앞뒤 창(s) · cos(π s)
//
//   옆 창   : 코사인 봉우리. 옆 입자들과 겹쳐 더해지면 끊김 없는 고리 하나가 된다
//   cos(πs) : 앞에 마루, 뒤에 골, 그 뒤에 작은 마루 — 물방울이 만든 물결의 잔물결
//   앞뒤 창 : 앞은 짧게, 뒤는 길게 줄어든다
//
//  겹친 입자는 가산 블렌드로 더해진다.
// =============================================================

cbuffer WaveSplatConstants : register(b0)
{
    float2 gOrigin;       // 영역 원점 (월드 X, Z)
    float  gTexelSize;    // 텍셀 하나의 월드 길이
    float  gTexels;       // 한 변의 텍셀 수
};

struct WaveParticle
{
    float2 position;
    float2 direction;
    float  amplitude;
    float  radius;
    float2 padding;
};

StructuredBuffer<WaveParticle> gParticles : register(t0);

struct Splat
{
    float4 position  : SV_POSITION;
    float2 local     : TEXCOORD0;    // (s, t)
    float  amplitude : TEXCOORD1;
};

static const float kPi = 3.14159265f;

static const float2 kCorners[6] =
{
    float2(-3.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f),
    float2(-3.0f, -1.0f), float2(1.0f,  1.0f), float2(-3.0f, 1.0f)
};

Splat VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    WaveParticle particle = gParticles[instanceId];
    float2 local = kCorners[vertexId];

    float2 side = float2(-particle.direction.y, particle.direction.x);
    float2 world = particle.position + (particle.direction * local.x + side * local.y) * particle.radius;

    // 월드 → 텍셀 → 클립. 텍스처의 행은 +Z 로 커지고, 렌더 타깃의 0 행은 클립 y = +1 이다
    float2 texel = (world - gOrigin) / gTexelSize;
    float2 ndc = texel / gTexels * 2.0f - 1.0f;

    Splat output;
    output.position = float4(ndc.x, -ndc.y, 0.0f, 1.0f);
    output.local = local;
    output.amplitude = particle.amplitude;
    return output;
}

float PSMain(Splat input) : SV_TARGET
{
    float s = input.local.x;
    float t = input.local.y;

    float across = 0.5f + 0.5f * cos(kPi * saturate(abs(t)));
    float along = s >= 0.0f ? 0.5f + 0.5f * cos(kPi * saturate(s))
                            : 0.5f + 0.5f * cos(kPi * saturate(-s / 3.0f));

    return input.amplitude * across * along * cos(kPi * s);
}
