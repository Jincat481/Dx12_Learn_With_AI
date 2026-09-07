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
};

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

float4 main(PSInput input) : SV_TARGET
{
    // 와이어프레임 모드에서는 선 자체가 도형이므로 단색으로 칠한다.
    if (gParams.z > 0.5f)
        return gColor;

    const bool useHeight = (gParams.w > 0.5f);

    // ---- 바탕색 ----
    float3 baseColor = gColor.rgb;
    if (useHeight)
    {
        float span = max(gHeightRange.y - gHeightRange.x, 0.0001f);
        float t = saturate((input.worldPos.y - gHeightRange.x) / span);
        baseColor = HeightColor(t);
    }

    // ---- 램버트 확산 조명 (S39) ----
    //  면이 빛을 정면으로 받을수록 밝다. N·L 이 그 값이다.
    //  gLightDir 은 빛이 "나아가는" 방향이라 광원 쪽을 보려면 부호를 뒤집는다.
    float3 normal = normalize(input.normal);
    float3 toLight = normalize(-gLightDir.xyz);

    float ambient = gLightDir.w;
    float diffuse = saturate(dot(normal, toLight));
    float lighting = saturate(ambient + diffuse * (1.0f - ambient));

    float3 rgb = baseColor * lighting;

    // ---- 격자선 ----
    //  하이트맵을 켜면 지형이 주인공이므로 선을 옅게 깔기만 한다.
    float minorLine = GridLine(input.uv * gParams.xy);
    float majorLine = GridLine(input.uv * gParams.xy * 0.1f);

    const float3 lineColor  = float3(0.55f, 0.75f, 0.95f);
    const float3 majorColor = float3(0.85f, 0.90f, 1.00f);

    float minorStrength = useHeight ? 0.10f : 0.55f;
    float majorStrength = useHeight ? 0.25f : 0.80f;

    rgb = lerp(rgb, lineColor, minorLine * minorStrength);
    rgb = lerp(rgb, majorColor, majorLine * majorStrength);

    return float4(rgb, 1.0f);
}
