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
Texture2D    gRippleHeight : register(t3); // 클릭 물결 높이 (S79)
Texture2D    gShoreMask  : register(t4);   // 해안 마스크, 1 = 벽 (S81)
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
// 물결 기울기 (S78)
//  수면 법선은 높이의 기울기로 만든다 : normal = normalize(-∂h/∂x, 1, -∂h/∂z)
//  여러 물결의 기울기를 더한 뒤 마지막에 한 번만 정규화한다.
//  d/dx sin(k·x) = k·cos(k·x) 이므로 사인파는 기울기를 바로 적을 수 있다.
// -------------------------------------------------------------
float2 SineSlope(float2 xz, float time, float2 direction, float frequency, float amplitude, float speed)
{
    direction = normalize(direction);
    float f = frequency * gWave.x;
    float phase = dot(direction, xz) * f + time * gWave.z * speed;
    return direction * amplitude * f * cos(phase);
}

// 너울 : 파장이 긴 두 파. 수면 전체가 천천히 오르내린다.
float2 SwellSlope(float2 xz, float time)
{
    return (SineSlope(xz, time, float2( 1.0f, 0.3f), 0.09f, 0.9f, 1.20f) +
            SineSlope(xz, time, float2(-0.4f, 1.0f), 0.13f, 0.7f, 1.55f)) * gWave.y;
}

// 흐름을 끈 비교용 : 짧은 사인파 네 개. 방향이 고정이라 규칙적인 무늬가 반복되어 보인다.
float2 SineDetailSlope(float2 xz, float time)
{
    return (SineSlope(xz, time, float2( 0.8f, -0.7f), 0.21f, 0.45f, 1.90f) +
            SineSlope(xz, time, float2(-1.0f, -0.2f), 0.33f, 0.30f, 2.25f) +
            SineSlope(xz, time, float2( 0.2f, -1.0f), 0.52f, 0.18f, 2.60f) +
            SineSlope(xz, time, float2( 0.6f,  0.9f), 0.80f, 0.10f, 2.95f)) * gWave.y;
}

// -------------------------------------------------------------
// 자연스러운 흐름 (S80)
//
//  1) 흐름장 : 컬 노이즈 (curl noise)
//     아무 노이즈나 속도로 쓰면 물이 한 점으로 모이거나(싱크) 솟아나는(소스) 곳이 생긴다.
//     스칼라 노이즈 ψ 의 기울기 (∂ψ/∂x, ∂ψ/∂z) 를 90도 돌린 (∂ψ/∂z, -∂ψ/∂x) 는
//     발산이 항상 0 이다. 들어온 만큼 나가므로 소용돌이치며 이어지는 해류처럼 보인다.
//
//  2) 흐름 맵 이류 (flow map advection)
//     무늬를 속도 방향으로 계속 밀면 시간이 갈수록 늘어지고 찢어진다.
//     그래서 반 주기씩 어긋난 두 벌을 일정 시간만 밀었다가 되돌리고,
//     각자 되돌아가는 순간에는 가중치 0 이 되게 삼각파로 섞는다. 둘의 가중치 합은 항상 1 이다.
//     되돌아갈 때마다 노이즈 좌표를 건너뛰어 같은 무늬가 반복되지 않게 한다.
// -------------------------------------------------------------
float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

// 값 노이즈와 그 기울기를 함께 구한다. (x : 값   yz : ∂/∂x, ∂/∂y)
float3 NoiseWithGradient(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);
    float2 du = 6.0f * f * (1.0f - f);

    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));

    float k = a - b - c + d;
    float value = a + (b - a) * u.x + (c - a) * u.y + k * u.x * u.y;
    float2 gradient = du * float2((b - a) + k * u.y, (c - a) + k * u.x);
    return float3(value, gradient);
}

float2 FbmGradient(float2 p, int octaves)
{
    float2 gradient = float2(0.0f, 0.0f);
    float amplitude = 1.0f;
    float frequency = 1.0f;

    [loop]
    for (int i = 0; i < octaves; ++i)
    {
        // noise(p · f) 를 p 로 미분하면 f 가 한 번 더 곱해진다 (연쇄 법칙)
        gradient += NoiseWithGradient(p * frequency + i * 17.0f).yz * amplitude * frequency;
        frequency *= 2.03f;
        amplitude *= 0.45f;
    }
    return gradient;
}

float2 CurrentVelocity(float2 xz)
{
    float2 g = FbmGradient(xz * 0.004f + float2(11.7f, 3.1f), 2);
    return float2(g.y, -g.x) * gFlow.y;   // 기울기를 90도 돌리면 발산 0
}

float2 FlowDetailSlope(float2 xz, float time, float depthFactor)
{
    // 얕은 곳은 바닥에 끌려 느리게 흐른다.
    float2 velocity = CurrentVelocity(xz) * lerp(0.15f, 1.0f, depthFactor);

    const float period = 3.0f;
    float cycle0 = time / period;
    float cycle1 = cycle0 + 0.5f;

    float phase0 = frac(cycle0);
    float phase1 = frac(cycle1);
    float weight0 = 1.0f - abs(1.0f - 2.0f * phase0);   // 0 → 1 → 0
    float weight1 = 1.0f - abs(1.0f - 2.0f * phase1);

    // 되돌아갈 때마다 노이즈의 다른 곳을 쓴다.
    float2 jump0 = floor(cycle0) * float2(2.59f, 4.27f);
    float2 jump1 = floor(cycle1) * float2(3.71f, 2.03f) + 13.1f;

    float2 p0 = (xz - velocity * phase0 * period) * gFlow.z + jump0;
    float2 p1 = (xz - velocity * phase1 * period) * gFlow.z + jump1;

    float2 slope = FbmGradient(p0, 3) * weight0 + FbmGradient(p1, 3) * weight1;

    // p 공간 기울기 → 월드 기울기 : 무늬 배율을 한 번 더 곱한다.
    return slope * gFlow.z * 0.35f * gWave.y;
}

// -------------------------------------------------------------
// 클릭 물결 (S79) : 시뮬레이션 텍스처의 높이를 중앙 차분해 기울기로 쓴다.
// -------------------------------------------------------------
float2 RippleSlope(float2 xz, out float height)
{
    height = 0.0f;
    if (gRipple.w <= 0.0f)
        return float2(0.0f, 0.0f);

    float2 uv = (xz - gRipple.xy) / gRipple.z;
    if (any(uv < 0.0f) || any(uv > 1.0f))
        return float2(0.0f, 0.0f);

    float texel = gFlow.w;
    height = gRippleHeight.SampleLevel(gClamp, uv, 0).r;

    float left  = gRippleHeight.SampleLevel(gClamp, uv - float2(texel, 0.0f), 0).r;
    float right = gRippleHeight.SampleLevel(gClamp, uv + float2(texel, 0.0f), 0).r;
    float down  = gRippleHeight.SampleLevel(gClamp, uv - float2(0.0f, texel), 0).r;
    float up    = gRippleHeight.SampleLevel(gClamp, uv + float2(0.0f, texel), 0).r;

    float worldTexel = texel * gRipple.z;
    return float2(right - left, up - down) / (2.0f * worldTexel) * gRipple.w;
}

float4 main(WaterPixel input) : SV_TARGET
{
    const float time = gEyePos.w;

    // SV_POSITION.xy 는 픽셀 좌표다. 화면 크기로 나누면 0~1 UV 가 된다.
    float2 screenUV = input.position.xy * gProjection.zw;

    float3 toEye = gEyePos.xyz - input.worldPos;
    float3 viewDir = toEye / max(length(toEye), 1e-4f);

    // ---- 물의 두께 (S77) ----
    float waterDepth = LinearDepth(input.position.z);
    float sceneDepth = LinearDepth(gSceneDepth.Sample(gClamp, screenUV).r);
    float thickness = max(sceneDepth - waterDepth, 0.0f);

    // ---- 수면 법선 : 너울 + 잔물결(흐름 또는 사인파) + 클릭 물결 ----
    float rippleHeight = 0.0f;
    float2 slope = SwellSlope(input.worldPos.xz, time);
    slope += (gFlow.x > 0.5f) ? FlowDetailSlope(input.worldPos.xz, time, saturate(thickness / 6.0f))
                              : SineDetailSlope(input.worldPos.xz, time);
    slope += RippleSlope(input.worldPos.xz, rippleHeight);

    float3 normal = normalize(float3(-slope.x, 1.0f, -slope.y));

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

    // 물결 높이 음영 : 마루는 밝게, 골은 어둡게.
    //  법선(기울기)만으로는 비스듬히 볼 때 카메라나 해를 향한 쪽 비탈만 빛나서
    //  동그란 물결이 한쪽으로만 흘러가는 반원처럼 보인다. 높이 자체를 밝기에 섞으면 원 전체가 보인다.
    color *= 1.0f + clamp(rippleHeight * 0.12f, -0.3f, 0.3f);
    color = lerp(color, float3(0.92f, 0.96f, 1.0f), saturate(rippleHeight * 0.3f - 0.1f) * 0.5f);

    color = ApplyAtmosphere(color, input.worldPos, gEyePos.xyz, gFogColor, gFogParams, gLightDir.xyz);

    // 두께가 0 에 가까운 가장자리는 바닥을 그대로 보여 준다. 수면이 칼로 자른 듯 끝나지 않게.
    if (gFlags.y > 0.5f)
        color = lerp(groundAtEdge, color, saturate(thickness / 0.35f));

    // 디버그 : 물결 높이를 색으로 (빨강 = 마루, 파랑 = 골). 간섭과 반사를 눈으로 확인한다.
    if (gFlags.w > 0.5f)
    {
        float3 debugColor = (rippleHeight > 0.0f) ? float3(1.0f, 0.25f, 0.2f) : float3(0.2f, 0.45f, 1.0f);
        color = lerp(color, debugColor, saturate(abs(rippleHeight) * 1.5f));

        // 시뮬레이션이 벽으로 쓰는 곳을 노랗게. 물결이 여기서 되돌아 나가야 한다.
        if (gShore.w > 0.5f)
        {
            float2 shoreUV = (input.worldPos.xz - gShore.xy) / gShore.z;
            if (all(shoreUV >= 0.0f) && all(shoreUV <= 1.0f))
            {
                float wall = step(0.5f, gShoreMask.SampleLevel(gClamp, shoreUV, 0).r);
                color = lerp(color, float3(1.0f, 0.85f, 0.1f), wall * 0.6f);
            }
        }
    }

    return float4(color, 1.0f);
}
