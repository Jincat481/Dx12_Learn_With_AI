// =============================================================
// WaterPS.hlsl - 물 픽셀 셰이더 (S76~S78)
//
//  물 한 픽셀의 색은 두 빛을 섞은 것이다.
//   - 반사 : 수면에 비친 하늘과 산 (반사 패스에서 미리 그려 둔 텍스처)
//   - 굴절 : 물속을 지나 보이는 바닥 (불투명을 다 그린 직후 복사해 둔 화면)
//  비율은 프레넬이 정한다. 발밑을 내려다보면 물속이, 멀리 비스듬히 보면 반사가 보인다.
//
//  깊이 복사본으로 "물이 얼마나 두꺼운가" 를 알 수 있다.
//   얕으면 바닥이 비치고, 깊으면 물색이 되고, 거의 0 이면 해안선 거품이 된다.
// =============================================================
#include "WaterCommon.hlsli"
#include "Atmosphere.hlsli"

Texture2D    gReflection : register(t0);   // 반사 패스 결과
Texture2D    gSceneColor : register(t1);   // 물을 그리기 직전 화면 색
Texture2D    gSceneDepth : register(t2);   // 물을 그리기 직전 깊이
SamplerState gClamp      : register(s0);

// 깊이 버퍼 값(0~1, 원근 나눗셈 뒤) → 카메라에서의 실제 거리
//  원근 투영의 깊이는 가까운 곳에 정밀도가 몰려 있어 선형이 아니다. 빼기 전에 되돌려야 한다.
float LinearDepth(float depth)
{
    float n = gProjection.x;
    float f = gProjection.y;
    return n * f / (f - depth * (f - n));
}

// -------------------------------------------------------------
// 물결 법선 (S78)
//  방향 · 파장 · 속도가 서로 다른 사인파 여섯 개를 겹친다.
//  높이 대신 기울기(미분)를 바로 더하면 법선이 나온다 : d/dx sin(k·x) = k·cos(k·x)
//  파장이 짧은 파일수록 진폭을 줄여야 잔물결이 큰 너울 위에 얹힌 모양이 된다.
// -------------------------------------------------------------
float3 WaveNormal(float2 xz, float time)
{
    const float2 directions[6] =
    {
        float2( 1.0f,  0.3f), float2(-0.4f,  1.0f), float2( 0.8f, -0.7f),
        float2(-1.0f, -0.2f), float2( 0.2f, -1.0f), float2( 0.6f,  0.9f),
    };
    const float frequencies[6] = { 0.09f, 0.13f, 0.21f, 0.33f, 0.52f, 0.80f };
    const float amplitudes[6]  = { 0.90f, 0.70f, 0.45f, 0.30f, 0.18f, 0.10f };

    float2 slope = float2(0.0f, 0.0f);

    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        float2 direction = normalize(directions[i]);
        float frequency = frequencies[i] * gWave.x;
        float phase = dot(direction, xz) * frequency + time * gWave.z * (1.2f + i * 0.35f);

        slope += direction * amplitudes[i] * frequency * cos(phase);
    }

    slope *= gWave.y;
    return normalize(float3(-slope.x, 1.0f, -slope.y));
}

float4 main(WaterPixel input) : SV_TARGET
{
    const float time = gEyePos.w;

    // SV_POSITION.xy 는 픽셀 좌표다. 화면 크기로 나누면 0~1 UV 가 된다.
    float2 screenUV = input.position.xy * gProjection.zw;

    float3 normal = WaveNormal(input.worldPos.xz, time);

    float3 toEye = gEyePos.xyz - input.worldPos;
    float3 viewDir = toEye / max(length(toEye), 1e-4f);

    // ---- 물의 두께 (S77) ----
    float waterDepth = LinearDepth(input.position.z);
    float sceneDepth = LinearDepth(gSceneDepth.Sample(gClamp, screenUV).r);
    float thickness = max(sceneDepth - waterDepth, 0.0f);

    // ---- 굴절 ----
    float3 refraction = lerp(gShallowColor.rgb, gDeepColor.rgb, 0.7f);
    float3 groundAtEdge = refraction;

    if (gFlags.y > 0.5f)
    {
        // 물결 법선만큼 뒤쪽 화면을 비틀어 읽는다. 얕은 곳은 덜 비튼다(바닥이 수면에 붙어 있으니까).
        float2 refractUV = screenUV + normal.xz * gWave.w * saturate(thickness / 3.0f);
        float refractDepth = LinearDepth(gSceneDepth.Sample(gClamp, refractUV).r);

        // 비틀어 읽은 곳이 물보다 앞에 있는 물체라면 그걸 물속에 끌어오면 안 된다. 비틀지 않는다.
        if (refractDepth < waterDepth)
        {
            refractUV = screenUV;
            refractDepth = sceneDepth;
        }

        float3 below = gSceneColor.Sample(gClamp, refractUV).rgb;
        groundAtEdge = gSceneColor.Sample(gClamp, screenUV).rgb;

        // 흡수 : 빛이 물속을 오래 지날수록 바닥 색은 사라지고 물 색만 남는다.
        float depthAmount = saturate((refractDepth - waterDepth) / max(gDeepColor.a, 1e-3f));
        float3 waterColor = lerp(gShallowColor.rgb, gDeepColor.rgb, depthAmount);
        refraction = lerp(below * 0.85f, waterColor, saturate(depthAmount * 1.1f + 0.2f));
    }

    // ---- 반사 (S76) ----
    //  반사 패스는 같은 카메라로 뒤집힌 세상을 그렸으므로 같은 화면 좌표에서 읽으면 된다.
    float3 reflection = gFogColor.rgb;
    if (gFlags.x > 0.5f)
        reflection = gReflection.Sample(gClamp, screenUV + normal.xz * 0.04f).rgb;

    // ---- 프레넬 (Schlick 근사) ----
    //  물의 수직 반사율은 약 2%. 비스듬할수록 (1 - N·V)^5 로 급격히 1 에 가까워진다.
    float NdotV = saturate(dot(normal, viewDir));
    float fresnel = 0.02f + 0.98f * pow(1.0f - NdotV, 5.0f);

    float3 color = lerp(refraction, reflection, fresnel);

    // ---- 햇빛 반짝임 (블린-퐁) ----
    float3 toSun = normalize(-gLightDir.xyz);
    float3 halfVector = normalize(toSun + viewDir);
    float sparkle = pow(saturate(dot(normal, halfVector)), 300.0f) * 4.0f;
    color += float3(1.0f, 0.95f, 0.85f) * sparkle;

    // ---- 해안선 거품 ----
    //  두께가 얇은 곳에 흰 띠를 두르고, 시간에 따라 흔들어 밀려왔다 빠지는 느낌을 준다.
    if (gFlags.z > 0.5f && gFlags.y > 0.5f)
    {
        float edge = 1.0f - saturate(thickness / 1.6f);
        float ripple = sin(input.worldPos.x * 0.9f + time * 1.3f) * sin(input.worldPos.z * 1.1f - time * 0.9f) * 0.5f + 0.5f;
        float foam = smoothstep(0.35f, 0.9f, edge * (0.6f + 0.4f * ripple));
        color = lerp(color, float3(0.95f, 0.97f, 1.0f), foam * 0.85f);
    }

    color = ApplyAtmosphere(color, input.worldPos, gEyePos.xyz, gFogColor, gFogParams, gLightDir.xyz);

    // 두께가 0 에 가까운 가장자리는 바닥을 그대로 보여 준다. 수면이 칼로 자른 듯 끝나지 않게.
    if (gFlags.y > 0.5f)
        color = lerp(groundAtEdge, color, saturate(thickness / 0.35f));

    return float4(color, 1.0f);
}
