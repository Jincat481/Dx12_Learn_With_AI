// =============================================================
// Waterfall.hlsl - 폭포 · 계류 (S90)
//
//  CPU 가 지형 위로 물줄기를 흘려 만든 리본 메시를 그린다.
//  정점마다 그 지점의 속도 · 공중 여부 · 낙하 거리 · 발원지에서의 거리를 함께 받는다.
//
//  한 픽셀의 색
//   = lerp(물빛, 흰 거품, 흰 정도) · 햇빛  + 반짝임
//  흰 정도는 빠를수록 · 공중일수록 · 무늬가 밝을수록 커진다.
//
//  무늬는 세 겹의 값 노이즈를 세로로 흘려 만든다. 빠른 곳일수록 빨리 흐른다.
//  공중으로 떨어지는 구간은 아래로 갈수록 무늬 문턱을 올려, 판이 물보라로 부서지게 한다.
//
//  지형과 만나는 선이 칼로 자른 듯 보이지 않게, 뒤에 이미 그려진 깊이를 읽어 가까울수록 흐리게 한다.
// =============================================================
#include "Atmosphere.hlsli"

cbuffer WaterfallConstants : register(b0)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4   gEyePos;      // xyz : 카메라   w : 시간(초)
    float4   gWaterColor;  // rgb : 물빛   a : 기본 불투명도
    float4   gFoamColor;   // rgb : 흰 거품 색   a : 거품 세기
    float4   gLightDir;    // xyz : 햇빛이 나아가는 방향
    float4   gProjection;  // x near  y far  z 1/화면폭  w 1/화면높이
    float4   gFogColor;
    float4   gFogParams;
    float4   gParams;      // x : 무늬 흐름 속도   y : 부서짐   z : 예비   w : 디버그 색
    float4   gCamRight;    // 카메라 오른쪽 (물보라 빌보드를 세우는 데 쓴다)
    float4   gCamUp;       // 카메라 위쪽
};

Texture2D    gSceneDepth : register(t0);   // 폭포를 그리기 직전의 깊이 (S77 의 복사본)
SamplerState gClamp      : register(s0);

struct VertexIn
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;   // x : 물줄기를 가로지르는 0~1   y : 흐른 거리 / 무늬 길이
    float4 flow     : TEXCOORD1;   // 물줄기  : x 속도(m/s)  y 공중(0/1)  z 떨어진 거리(m)  w 흐른 거리(m)
                                   // 물보라  : x 크기(m)    y 떠오르는 높이(m)  z 한 번 도는 속도  w 씨앗
    float4 kind     : TEXCOORD2;   // x : 0 물줄기 / 1 물보라   yz : 사각형 모서리(-1~1)
};

struct PixelIn
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 flow     : TEXCOORD3;
    float  viewZ    : TEXCOORD4;   // 카메라에서의 거리 (깊이 비교용)
    float2 mist     : TEXCOORD5;   // x : 물보라인가   y : 지금 진하기
};

PixelIn VSMain(VertexIn input)
{
    PixelIn output;

    float3 world = mul(float4(input.position, 1.0f), gWorld).xyz;
    float3 normal = normalize(mul(float4(input.normal, 0.0f), gWorld).xyz);
    float mistAmount = 0.0f;

    // ---- 물보라 : 한 점에 모인 정점 네 개를 카메라를 향한 사각형으로 편다 ----
    //  떠오르며 커지고 옅어지는 한살이를 씨앗마다 어긋나게 돌린다.
    if (input.kind.x > 0.5f)
    {
        float phase = frac(gEyePos.w * input.flow.z + input.flow.w);
        float size = input.flow.x * (0.55f + 0.85f * phase);

        world += float3(0.0f, 1.0f, 0.0f) * (input.flow.y * phase);
        world += (gCamRight.xyz * input.kind.y + gCamUp.xyz * input.kind.z) * size;

        mistAmount = sin(phase * 3.14159265f);   // 피어올랐다가 사라진다
        normal = normalize(gEyePos.xyz - world);
    }

    // 리본은 월드 좌표로 만들어 두므로 gWorld 는 단위 행렬이다 (gWVP = 뷰 · 투영)
    float4 clip = mul(float4(world, 1.0f), gWVP);

    output.position = clip;
    output.worldPos = world;
    output.normal = normal;
    output.uv = input.uv;
    output.flow = input.flow;
    output.viewZ = clip.w;
    output.mist = float2(input.kind.x, mistAmount);
    return output;
}

// ---- 값 노이즈 : 격자점마다 난수를 하나 두고 부드럽게 섞는다 ----
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float ValueNoise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    float a = Hash(cell);
    float b = Hash(cell + float2(1.0f, 0.0f));
    float c = Hash(cell + float2(0.0f, 1.0f));
    float d = Hash(cell + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// 원근 깊이 → 카메라에서의 실제 거리
float LinearDepth(float depth, float nearZ, float farZ)
{
    return nearZ * farZ / (farZ - depth * (farZ - nearZ));
}

float4 PSMain(PixelIn input) : SV_TARGET
{
    const float time = gEyePos.w;

    // ---- 물보라 : 둥글게 옅어지는 얼룩 ----
    if (input.mist.x > 0.5f)
    {
        float2 local = input.uv * 2.0f - 1.0f;
        float radial = saturate(1.0f - dot(local, local));
        float puff = ValueNoise(input.uv * 4.0f + float2(input.flow.w * 13.0f, time * 0.35f)) * 0.5f + 0.5f;

        float mistAlpha = radial * radial * puff * input.mist.y * 0.32f * gFoamColor.a;

        float2 screenUVMist = input.position.xy * gProjection.zw;
        float sceneZMist = LinearDepth(gSceneDepth.SampleLevel(gClamp, screenUVMist, 0).r, gProjection.x, gProjection.y);
        mistAlpha *= saturate((sceneZMist - input.viewZ) / 2.0f);

        float3 mistColor = ApplyAtmosphere(gFoamColor.rgb, input.worldPos, gEyePos.xyz, gFogColor, gFogParams, gLightDir.xyz);
        return float4(mistColor, saturate(mistAlpha));
    }
    const float speed = input.flow.x;
    const float airborne = input.flow.y;
    const float fallen = input.flow.z;
    const float travel = input.flow.w;

    // ---- 흘러내리는 무늬 : 세 겹을 서로 다른 속도로 ----
    //  빠른 구간일수록 무늬가 빨리 지나가야 물이 가속하는 것처럼 보인다.
    float flowSpeed = (1.2f + speed * 0.45f) * gParams.x;
    float2 uvA = float2(input.uv.x *  7.0f,            input.uv.y * 1.0f - time * flowSpeed * 0.22f);
    float2 uvB = float2(input.uv.x * 17.0f +  3.7f,    input.uv.y * 2.3f - time * flowSpeed * 0.38f);
    float2 uvC = float2(input.uv.x * 33.0f +  8.1f,    input.uv.y * 4.1f - time * flowSpeed * 0.55f);
    float streak = ValueNoise(uvA) * 0.50f + ValueNoise(uvB) * 0.32f + ValueNoise(uvC) * 0.18f;

    // ---- 모양 다듬기 ----
    float edge = 1.0f - pow(saturate(abs(input.uv.x * 2.0f - 1.0f)), 2.5f);   // 가장자리는 얇아진다
    float birth = saturate(travel / 3.0f);                                     // 발원지에서 서서히 시작

    // 공중 구간은 떨어질수록 부서진다 : 무늬 문턱을 올려 판을 물보라로 흩는다
    float breakup = airborne * saturate(fallen / 7.0f) * gParams.y;
    float dissolve = smoothstep(breakup * 0.9f, breakup * 0.9f + 0.35f, streak + 0.3f);

    float white = saturate(0.25f + saturate(speed / 10.0f) * 0.5f + airborne * 0.25f + streak * 0.4f) * gFoamColor.a;

    float3 color = lerp(gWaterColor.rgb, gFoamColor.rgb, white);

    // ---- 빛 ----
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(gEyePos.xyz - input.worldPos);
    float3 toSun = -gLightDir.xyz;

    // 얇은 물이라 뒤에서 오는 빛도 비친다(양면). 그래서 |dot| 을 쓴다.
    float diffuse = abs(dot(normal, toSun)) * 0.35f + 0.75f;
    float3 halfVector = normalize(toSun + viewDir);
    float specular = pow(saturate(abs(dot(normal, halfVector))), 48.0f) * (0.25f + white * 0.75f);

    color = color * diffuse + specular;

    float alpha = gWaterColor.a * edge * birth * dissolve * (0.55f + 0.55f * streak);

    // ---- 지형과 만나는 선 부드럽게 (S77 의 깊이 복사본) ----
    //  지면을 타는 구간은 지형에서 30 cm 밖에 안 떠 있다. 페이드 거리를 길게 잡으면 물줄기가 통째로 사라진다.
    float2 screenUV = input.position.xy * gProjection.zw;
    float sceneZ = LinearDepth(gSceneDepth.SampleLevel(gClamp, screenUV, 0).r, gProjection.x, gProjection.y);
    alpha *= saturate((sceneZ - input.viewZ) / 0.2f);

    if (gParams.w > 0.5f)
    {
        // 디버그 : 공중 구간 빨강 · 지면을 타는 구간 파랑
        color = lerp(float3(0.2f, 0.5f, 1.0f), float3(1.0f, 0.3f, 0.2f), airborne);
        alpha = saturate(alpha + 0.35f);
    }

    color = ApplyAtmosphere(color, input.worldPos, gEyePos.xyz, gFogColor, gFogParams, gLightDir.xyz);
    return float4(color, saturate(alpha));
}
