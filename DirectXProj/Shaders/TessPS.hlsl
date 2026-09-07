// =============================================================
// TessPS.hlsl - 테셀레이션 지형 픽셀 셰이더 (스텝 7)
//  스텝 2 의 높이 색상 램프와 램버트 조명을 그대로 쓴다.
//  테셀레이션 자체를 보여주는 것이 목적이라 단순하게 유지한다.
// =============================================================
#include "TessCommon.hlsli"

float3 HeightColor(float t)
{
    const float3 lowland = float3(0.22f, 0.34f, 0.22f);
    const float3 grass   = float3(0.38f, 0.50f, 0.28f);
    const float3 rock    = float3(0.50f, 0.46f, 0.41f);
    const float3 snow    = float3(0.93f, 0.95f, 0.98f);

    float3 rgb = lerp(lowland, grass, smoothstep(0.00f, 0.40f, t));
    rgb = lerp(rgb, rock, smoothstep(0.45f, 0.75f, t));
    rgb = lerp(rgb, snow, smoothstep(0.82f, 1.00f, t));
    return rgb;
}

float4 main(DomainOutput input) : SV_TARGET
{
    if (gParams.x > 0.5f)
        return float4(0.55f, 0.85f, 0.95f, 1.0f);   // 와이어프레임

    float span = max(gHeightRange.y - gHeightRange.x, 0.0001f);
    float t = saturate((input.worldPos.y - gHeightRange.x) / span);

    float3 baseColor = HeightColor(t);

    float3 normal = normalize(input.normal);
    float3 toLight = normalize(-gLightDir.xyz);

    float ambient = gLightDir.w;
    float diffuse = saturate(dot(normal, toLight));
    float lighting = saturate(ambient + diffuse * (1.0f - ambient));

    return float4(baseColor * lighting, 1.0f);
}
