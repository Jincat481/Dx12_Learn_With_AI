// =============================================================
// TerrainPS.hlsl - 터레인 픽셀 셰이더 (스텝 1)
//  아직 텍스처도 조명도 없다. UV 로 격자선만 그려서
//  메시가 몇 칸으로 나뉘어 있는지 눈으로 보이게 한다.
// =============================================================

cbuffer TerrainConstants : register(b0)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4   gColor;         // 평면 모드의 기본 색
    float4   gParams;        // x,y : 격자 칸 수   z : 와이어프레임   w : 높이 사용(0/1)
    float4   gHeightRange;   // x : 최저 높이   y : 최고 높이
    float4   gLightDir;      // xyz : 방향광이 나아가는 방향   w : 환경광 세기
    float4   gSplat;         // x : 타일 반복 횟수   y : 스플래팅 사용   z : 디버그 단색
};

// 스플래팅 레이어 (S44)
Texture2D    gDirt  : register(t0);
Texture2D    gGrass : register(t1);
Texture2D    gRock  : register(t2);
Texture2D    gSnow  : register(t3);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

// 화면 기준으로 굵기가 일정한 격자선을 그린다.
//  fwidth 는 옆 픽셀과의 변화량이라, 멀리 있는 칸일수록 자동으로 얇아지지 않고
//  일정한 굵기를 유지하며 계단 현상도 줄어든다.
float GridLine(float2 coord)
{
    float2 grid = abs(frac(coord - 0.5f) - 0.5f) / max(fwidth(coord), 1e-6f);
    return 1.0f - saturate(min(grid.x, grid.y));
}

// 높이에 따라 색을 고른다. (S40)
//  0 = 가장 낮은 곳, 1 = 가장 높은 곳
float3 HeightColor(float t)
{
    const float3 lowland = float3(0.22f, 0.34f, 0.22f);   // 낮은 지대 (짙은 풀)
    const float3 grass   = float3(0.38f, 0.50f, 0.28f);   // 풀밭
    const float3 rock    = float3(0.50f, 0.46f, 0.41f);   // 바위
    const float3 snow    = float3(0.93f, 0.95f, 0.98f);   // 눈

    float3 rgb = lerp(lowland, grass, smoothstep(0.00f, 0.40f, t));
    rgb = lerp(rgb, rock, smoothstep(0.45f, 0.75f, t));
    rgb = lerp(rgb, snow, smoothstep(0.82f, 1.00f, t));
    return rgb;
}

// -------------------------------------------------------------
// 텍스처 스플래팅 (S44, S45, S47)
//  네 장을 "가중치" 로 섞는다. 가중치는 두 가지에서 나온다.
//   - 높이  : 낮으면 흙, 중간이면 풀, 아주 높으면 눈
//   - 경사도: 가파르면 바위 (절벽에는 풀이 붙지 않는다)
//
//  경사도는 법선의 y 성분으로 바로 구한다.
//      평지면 normal.y = 1  →  slope = 0
//      절벽이면 normal.y = 0 →  slope = 1
// -------------------------------------------------------------
float3 SplatColor(float2 uv, float height01, float slope)
{
    float2 tiledUV = uv * gSplat.x;

    float3 dirt  = gDirt .Sample(gSampler, tiledUV).rgb;
    float3 grass = gGrass.Sample(gSampler, tiledUV).rgb;
    float3 rock  = gRock .Sample(gSampler, tiledUV).rgb;
    float3 snow  = gSnow .Sample(gSampler, tiledUV).rgb;

    // 높이 기반 가중치. smoothstep 으로 경계를 부드럽게 넘긴다.
    float wDirt  = 1.0f - smoothstep(0.05f, 0.30f, height01);
    float wGrass = smoothstep(0.08f, 0.32f, height01) * (1.0f - smoothstep(0.58f, 0.82f, height01));
    float wSnow  = smoothstep(0.74f, 0.93f, height01);

    // 경사 기반 가중치. 완만한 비탈에서도 바위가 드러나도록 임계값을 낮게 잡는다.
    float wRock = smoothstep(0.10f, 0.38f, slope);

    // 바위가 차지한 만큼 나머지를 눌러 준다.
    float remain = 1.0f - wRock;
    wDirt  *= remain;
    wGrass *= remain;
    wSnow  *= remain;

    // 가중치 합을 1로 맞춘다. 이걸 빼먹으면 겹치는 구간이 밝아지거나 어두워진다.
    float total = max(wDirt + wGrass + wRock + wSnow, 1e-4f);

    return (dirt * wDirt + grass * wGrass + rock * wRock + snow * wSnow) / total;
}

float4 main(PSInput input) : SV_TARGET
{
    // 와이어프레임 모드에서는 선 자체가 도형이므로 단색으로 칠한다.
    if (gParams.z > 0.5f)
        return gColor;

    const bool useHeight = (gParams.w > 0.5f);
    const bool useSplat  = (gSplat.y > 0.5f);

    float3 normal = normalize(input.normal);

    float span = max(gHeightRange.y - gHeightRange.x, 0.0001f);
    float height01 = saturate((input.worldPos.y - gHeightRange.x) / span);

    // ---- 바탕색 ----
    //  디버그 뷰(청크 색 / LOD 색)에서는 넘겨준 색을 그대로 쓴다.
    float3 baseColor = gColor.rgb;
    if (gSplat.z > 0.5f)
    {
        baseColor = gColor.rgb;
    }
    else if (useSplat)
    {
        float slope = 1.0f - saturate(normal.y);
        baseColor = SplatColor(input.uv, height01, slope);
    }
    else if (useHeight)
    {
        baseColor = HeightColor(height01);
    }

    // ---- 램버트 확산 조명 (S39) ----
    float3 toLight = normalize(-gLightDir.xyz);

    float ambient = gLightDir.w;
    float diffuse = saturate(dot(normal, toLight));
    float lighting = saturate(ambient + diffuse * (1.0f - ambient));

    float3 rgb = baseColor * lighting;

    // ---- 격자선 ----
    //  스플래팅을 켜면 텍스처가 주인공이므로 격자선은 끈다.
    if (!useSplat)
    {
        float minorLine = GridLine(input.uv * gParams.xy);
        float majorLine = GridLine(input.uv * gParams.xy * 0.1f);

        const float3 lineColor  = float3(0.55f, 0.75f, 0.95f);
        const float3 majorColor = float3(0.85f, 0.90f, 1.00f);

        float minorStrength = useHeight ? 0.10f : 0.55f;
        float majorStrength = useHeight ? 0.25f : 0.80f;

        rgb = lerp(rgb, lineColor, minorLine * minorStrength);
        rgb = lerp(rgb, majorColor, majorLine * majorStrength);
    }

    return float4(rgb, 1.0f);
}
