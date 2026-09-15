// =============================================================
// WaterPS.hlsl - 바다 수면 픽셀 셰이더 (S76~S81, S85~S87)
//
//  바다 한 픽셀의 색
//   = lerp(물 몸체 색, 반사, 프레넬) + 햇빛 반사광 + 흰 파도머리
//
//  - 법선 : 파도 기울기 텍스처 3단의 합. 게르스트너로 수평으로 밀린 만큼 기울기를 늘여 준다
//  - 물 몸체 : 깊은 남색. 파도 마루는 햇빛이 물을 통과해 청록빛으로 비친다(표면 아래 산란)
//  - 반사 : 반사 패스에서 그려 둔 하늘과 섬. 비스듬히 볼수록 강하다(프레넬)
//  - 얕은 물 : 깊이 복사본으로 두께를 알아 바닥이 비치게 한다 (S77)
//  - 거품 : 파도가 접히는 곳(야코비안이 작은 곳)에서 생겨 흩어진다 (S86)
// =============================================================
#include "WaterCommon.hlsli"
#include "Atmosphere.hlsli"

Texture2D    gReflection    : register(t0);   // 반사 패스 결과
Texture2D    gSceneColor    : register(t1);   // 물을 그리기 직전 화면 색
Texture2D    gSceneDepth    : register(t2);   // 물을 그리기 직전 깊이
Texture2D    gRippleHeight  : register(t3);   // 클릭 물결 높이 (S79)
Texture2D    gShoreMask     : register(t4);   // 해안 높이 맵 : 땅 높이 - 수위 (S81)
Texture2D    gDisplacement0 : register(t5);   // 파도 변위 + 거품 (S85)
Texture2D    gDisplacement1 : register(t6);
Texture2D    gDisplacement2 : register(t7);
Texture2D    gSlope0        : register(t8);   // 파도 기울기
Texture2D    gSlope1        : register(t9);
Texture2D    gSlope2        : register(t10);
SamplerState gClamp : register(s0);
SamplerState gWrap  : register(s1);

// 깊이 버퍼 값(0~1) → 카메라에서의 실제 거리 (S77)
float LinearDepth(float depth)
{
    float n = gProjection.x;
    float f = gProjection.y;
    return n * f / (f - depth * (f - n));
}

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);

    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// 거품 무늬 : 거품은 매끈한 흰 판이 아니라 구멍 난 거미줄 같은 모양이다
float FoamPattern(float2 xz, float time)
{
    float2 p = xz * 1.3f + float2(time * 0.05f, time * 0.03f);
    return ValueNoise(p) * 0.55f + ValueNoise(p * 2.3f + 7.1f) * 0.30f + ValueNoise(p * 5.1f + 3.7f) * 0.15f;
}

// 클릭 물결 (S79) : 시뮬레이션 텍스처의 높이를 중앙 차분해 기울기로 쓴다.
float2 RippleSlope(float2 xz, out float height)
{
    height = 0.0f;
    if (gRipple.w <= 0.0f)
        return float2(0.0f, 0.0f);

    float2 uv = (xz - gRipple.xy) / gRipple.z;
    if (any(uv < 0.0f) || any(uv > 1.0f))
        return float2(0.0f, 0.0f);

    float texel = gOcean.w;
    height = gRippleHeight.SampleLevel(gClamp, uv, 0).r;

    float left  = gRippleHeight.SampleLevel(gClamp, uv - float2(texel, 0.0f), 0).r;
    float right = gRippleHeight.SampleLevel(gClamp, uv + float2(texel, 0.0f), 0).r;
    float down  = gRippleHeight.SampleLevel(gClamp, uv - float2(0.0f, texel), 0).r;
    float up    = gRippleHeight.SampleLevel(gClamp, uv + float2(0.0f, texel), 0).r;

    float worldTexel = texel * gRipple.z;
    return float2(right - left, up - down) / (2.0f * worldTexel) * gRipple.w * gOcean2.y;
}

float4 main(WaterPixel input) : SV_TARGET
{
    const float time = gEyePos.w;

    float2 screenUV = input.position.xy * gProjection.zw;

    float3 toEye = gEyePos.xyz - input.worldPos;
    float eyeDistance = length(toEye);
    float3 viewDir = toEye / max(eyeDistance, 1e-4f);

    // ---- 물의 두께 (S77) ----
    float waterDepth = LinearDepth(input.position.z);
    float sceneDepth = LinearDepth(gSceneDepth.Sample(gClamp, screenUV).r);
    float thickness = max(sceneDepth - waterDepth, 0.0f);

    float2 xz = input.ocean.xy;
    float calm = input.ocean.z;
    float waveHeight = input.ocean.w;

    // ---- 파도 법선 (S85) ----
    //  밉맵이 있는 텍스처를 Sample 로 읽어 멀리서는 알아서 평균이 된다.
    float fade1 = 1.0f - smoothstep(500.0f, 1000.0f, eyeDistance);
    float fade2 = 1.0f - smoothstep(150.0f, 350.0f, eyeDistance);

    float4 slopeSum = (gSlope0.Sample(gWrap, xz / gOcean.x) +
                       gSlope1.Sample(gWrap, xz / gOcean.y) * fade1 +
                       gSlope2.Sample(gWrap, xz / gOcean.z) * fade2) * calm;

    float rippleHeight = 0.0f;
    float2 slope = slopeSum.xy + RippleSlope(xz, rippleHeight);

    // 게르스트너로 점들이 마루에 몰리면 같은 높이 차가 더 짧은 거리에서 일어난다 → 기울기를 늘여 준다
    float2 stretch = max(1.0f + slopeSum.zw, 0.25f);
    float3 normal = normalize(float3(-slope.x / stretch.x, 1.0f, -slope.y / stretch.y));

    // 800 m 타일은 텍셀 하나가 3 m 라 거품이 흐릿한 회색 덩어리가 된다. 거품은 촘촘한 두 타일에서만 읽는다.
    float foamAmount = (gDisplacement1.Sample(gWrap, xz / gOcean.y).w * fade1 +
                        gDisplacement2.Sample(gWrap, xz / gOcean.z).w * fade2 * 0.6f) * calm * gOcean2.w;

    float3 toSun = normalize(-gLightDir.xyz);
    float sunHeight = saturate(toSun.y);
    float NdotL = saturate(dot(normal, toSun));
    float NdotV = saturate(dot(normal, viewDir));

    // ---- 물 몸체 (S87) ----
    //  바다는 빛을 거의 삼켜 깊은 남색이다. 다만 파도 마루는 얇아서 햇빛이 통과하며 청록으로 비친다.
    //  해를 향해(역광으로) 볼수록, 마루가 높을수록 강하다.
    const float3 scatterColor = float3(0.03f, 0.34f, 0.32f);
    float crest = saturate(waveHeight / max(gOcean2.x, 0.1f) + 0.35f);
    float2 lookFlat = normalize(-viewDir.xz + 1e-5f);
    float2 sunFlat = normalize(toSun.xz + 1e-5f);
    float backLight = pow(saturate(dot(lookFlat, sunFlat)), 3.0f);
    float3 body = gDeepColor.rgb * (0.45f + 0.55f * sunHeight) +
                  scatterColor * crest * (0.18f + 0.9f * backLight) * (0.35f + 0.65f * sunHeight) * (0.5f + 0.5f * (1.0f - NdotV));

    // ---- 굴절 / 얕은 물 ----
    float3 refraction = body;
    float3 groundAtEdge = body;

    if (gFlags.y > 0.5f)
    {
        float2 refractUV = screenUV + normal.xz * gWave.w * saturate(thickness / 3.0f);
        float refractDepth = LinearDepth(gSceneDepth.Sample(gClamp, refractUV).r);

        // 비틀어 읽은 곳이 물보다 앞에 있는 물체라면 물속에 끌어오지 않는다.
        if (refractDepth < waterDepth)
        {
            refractUV = screenUV;
            refractDepth = sceneDepth;
        }

        float3 below = gSceneColor.Sample(gClamp, refractUV).rgb;
        groundAtEdge = gSceneColor.Sample(gClamp, screenUV).rgb;

        // 흡수 : 빛이 물속을 오래 지날수록 바닥 색은 사라지고 바다 색만 남는다
        float depthAmount = saturate((refractDepth - waterDepth) / max(gDeepColor.a, 1e-3f));
        float3 shallow = lerp(below * 0.8f, gShallowColor.rgb, 0.45f);
        refraction = lerp(shallow, body, smoothstep(0.0f, 1.0f, depthAmount));
    }

    // ---- 반사 (S76) ----
    float3 reflection = gFogColor.rgb;
    if (gFlags.x > 0.5f)
    {
        float distortion = 0.01f + 0.05f * (1.0f - smoothstep(50.0f, 600.0f, eyeDistance));
        reflection = gReflection.Sample(gClamp, screenUV + normal.xz * distortion).rgb;
    }

    // 파도 뒷면처럼 반사 방향이 수면 아래를 향하면 하늘이 아니라 물이 비친다
    float3 reflected = reflect(-viewDir, normal);
    reflection = lerp(body, reflection, saturate(reflected.y * 4.0f + 0.25f));

    // ---- 프레넬 (Schlick) : 물의 수직 반사율 2% ----
    float fresnel = 0.02f + 0.98f * pow(1.0f - NdotV, 5.0f);
    float3 color = lerp(refraction, reflection, fresnel);

    // ---- 햇빛 반사광 ----
    //  가까이는 날카롭게, 멀리는 넓게 번진다(멀리 있는 픽셀 하나에 파도 여러 개가 들어가므로)
    float3 halfVector = normalize(toSun + viewDir);
    float shininess = lerp(900.0f, 80.0f, saturate(eyeDistance / 800.0f));
    float specular = pow(saturate(dot(normal, halfVector)), shininess) * (shininess + 8.0f) / 25.0f;
    color += float3(1.0f, 0.95f, 0.85f) * specular * 0.08f * smoothstep(0.0f, 0.2f, toSun.y);

    // ---- 흰 파도머리 (S86) ----
    float pattern = FoamPattern(xz, time);
    //  거품은 밝게 끊어진 무늬라야 한다. 흐릿하게 섞으면 수면 위에 구름이 뜬 것처럼 보인다.
    float whitecap = smoothstep(0.35f, 0.6f, foamAmount * (0.2f + 1.1f * pattern));
    float3 foamColor = float3(0.93f, 0.95f, 0.97f) * (0.8f + 0.2f * NdotL);
    color = lerp(color, foamColor, whitecap);

    // ---- 해안선 거품 ----
    if (gFlags.z > 0.5f && gFlags.y > 0.5f)
    {
        float edge = 1.0f - saturate(thickness / 1.6f);
        float foam = smoothstep(0.35f, 0.9f, edge * (0.55f + 0.6f * pattern));
        color = lerp(color, foamColor, foam * 0.85f);
    }

    // 클릭 물결 마루는 살짝 밝게
    color = lerp(color, float3(0.92f, 0.96f, 1.0f), saturate(rippleHeight * gOcean2.y * 0.6f - 0.1f) * 0.4f);

    color = ApplyAtmosphere(color, input.worldPos, gEyePos.xyz, gFogColor, gFogParams, gLightDir.xyz);

    // 두께가 0 에 가까운 가장자리는 바닥을 그대로 보여 준다
    if (gFlags.y > 0.5f)
        color = lerp(groundAtEdge, color, saturate(thickness / 0.35f));

    // 디버그 : 클릭 물결 높이를 색으로 (빨강 = 마루, 파랑 = 골), 해안 벽은 노랑
    if (gFlags.w > 0.5f)
    {
        float3 debugColor = (rippleHeight > 0.0f) ? float3(1.0f, 0.25f, 0.2f) : float3(0.2f, 0.45f, 1.0f);
        color = lerp(color, debugColor, saturate(abs(rippleHeight) * 1.5f));

        if (gShore.w > 0.5f)
        {
            float2 shoreUV = (xz - gShore.xy) / gShore.z;
            if (all(shoreUV >= 0.0f) && all(shoreUV <= 1.0f))
            {
                float wall = step(0.0f, gShoreMask.SampleLevel(gClamp, shoreUV, 0).r);
                color = lerp(color, float3(1.0f, 0.85f, 0.1f), wall * 0.6f);
            }
        }
    }

    return float4(color, 1.0f);
}
