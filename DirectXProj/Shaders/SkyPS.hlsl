// =============================================================
// SkyPS.hlsl - 하늘 + 동적 왜곡 구름 (스텝 8, 9 / S55~S57)
//
//  하늘  : 보는 방향의 높이(y)로 지평선색 ~ 천정색을 섞는다.
//  태양  : 태양 방향과의 내적으로 밝은 점과 주변 번짐을 만든다.
//  구름  : 노이즈를 노이즈로 한 번 더 밀어(domain warping) 뭉게뭉게한 모양을 만든다.
// =============================================================

cbuffer SkyConstants : register(b0)
{
    float4x4 gWVP;
    float4   gHorizonColor;
    float4   gZenithColor;
    float4   gSunDirection;   // xyz : 빛이 나아가는 방향
    float4   gParams;         // x : 시간   y : 구름 사용   z : 구름 양   w : 구름 속도
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float3 direction : TEXCOORD0;
};

// ---- 값 노이즈 (S56) ----
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);   // smoothstep 보간

    float a = Hash(i);
    float b = Hash(i + float2(1.0f, 0.0f));
    float c = Hash(i + float2(0.0f, 1.0f));
    float d = Hash(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float FBM(float2 p)
{
    float total = 0.0f;
    float amplitude = 0.5f;

    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        total += ValueNoise(p) * amplitude;
        p *= 2.02f;              // 주파수를 키우고
        amplitude *= 0.5f;       // 진폭을 줄인다
    }
    return total;
}

// ---- 동적 왜곡 구름 (S57) ----
//  noise(p) 를 그대로 쓰면 규칙적인 얼룩처럼 보인다.
//  좌표 자체를 다른 노이즈로 밀어 두면(domain warping) 소용돌이치는 구름이 된다.
float CloudDensity(float2 p, float time, float speed)
{
    float2 drift = float2(time * speed, time * speed * 0.35f);

    // 1차 왜곡 : 어디를 샘플링할지 자체를 흔든다
    float2 q = float2(FBM(p + drift),
                      FBM(p + drift + float2(5.2f, 1.3f)));

    // 2차 왜곡 : 한 번 더 흔들면 뭉게구름의 결이 생긴다
    float2 r = float2(FBM(p + 4.0f * q + float2(1.7f, 9.2f) + drift * 0.6f),
                      FBM(p + 4.0f * q + float2(8.3f, 2.8f) + drift * 0.4f));

    return FBM(p + 4.0f * r);
}

float4 main(PSInput input) : SV_TARGET
{
    float3 direction = normalize(input.direction);

    // ---- 하늘 그라데이션 ----
    //  지평선(0) ~ 천정(1). pow 로 지평선 쪽을 조금 더 두껍게 만든다.
    float height = saturate(direction.y);
    float t = pow(height, 0.55f);
    float3 sky = lerp(gHorizonColor.rgb, gZenithColor.rgb, t);

    // ---- 태양 ----
    float3 toSun = normalize(-gSunDirection.xyz);
    float sunDot = saturate(dot(direction, toSun));

    float sunDisc = smoothstep(0.9975f, 0.9995f, sunDot);   // 동그란 해
    float sunGlow = pow(sunDot, 24.0f) * 0.45f;             // 주변 번짐

    sky += float3(1.0f, 0.94f, 0.80f) * sunGlow;
    sky = lerp(sky, float3(1.0f, 0.97f, 0.88f), sunDisc);

    // ---- 구름 ----
    if (gParams.y > 0.5f)
    {
        // 하늘을 평평한 판으로 보고 방향을 투영한다.
        //  지평선 근처에서 y 가 0 에 가까워지면 좌표가 발산하므로 하한을 둔다.
        float horizon = max(direction.y, 0.06f);
        float2 cloudUV = direction.xz / horizon * 0.35f;

        float density = CloudDensity(cloudUV, gParams.x, gParams.w);

        // 구름 양(coverage)을 기준으로 잘라 낸다. 값이 클수록 하늘이 덮인다.
        float coverage = gParams.z;
        float cloud = smoothstep(1.0f - coverage, 1.0f - coverage + 0.28f, density);

        // 지평선 근처에서는 서서히 사라지게 해 잘린 티를 없앤다.
        cloud *= smoothstep(0.0f, 0.22f, direction.y);

        // 두꺼운 부분은 어둡게, 얇은 부분은 밝게 (아주 단순한 셀프 섀도)
        float3 cloudColor = lerp(float3(0.62f, 0.66f, 0.74f),
                                 float3(1.0f, 1.0f, 1.0f),
                                 saturate(density * 1.3f));

        sky = lerp(sky, cloudColor, cloud);
    }

    return float4(sky, 1.0f);
}
