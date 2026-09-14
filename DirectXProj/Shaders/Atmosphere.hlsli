// =============================================================
// Atmosphere.hlsli - 거리 안개 · 대기 원근 (S64)
//
//  멀리 있는 것은 공기층을 더 많이 지나 온다. 그만큼
//   - 원래 색은 흡수·산란되어 약해지고 (감쇠)
//   - 하늘빛이 섞여 들어온다 (내산란)
//  그래서 먼 산은 흐리고 푸르게 보인다. 이것이 대기 원근이다.
//
//  안개 색은 하늘의 지평선 색과 **같은 식**으로 만들어야 한다.
//  그래야 지형이 끝나는 곳이 하늘에 이음매 없이 녹아든다.
//  (태양 번짐의 지수 24 와 세기는 SkyPS.hlsl 과 같은 값이다)
// =============================================================
#ifndef ATMOSPHERE_HLSLI
#define ATMOSPHERE_HLSLI

// fogColor  : rgb 지평선 색   a 켜짐(0/1)
// fogParams : x 시작 거리   y 끝 거리   z 밀도   w 태양 산란 세기
float3 ApplyAtmosphere(float3 rgb, float3 worldPos, float3 eyePos,
                       float4 fogColor, float4 fogParams, float3 lightDir)
{
    if (fogColor.a < 0.5f)
        return rgb;

    float3 toPixel = worldPos - eyePos;
    float distance = length(toPixel);
    float3 viewDir = toPixel / max(distance, 1e-4f);

    // 지수 안개 : 지나온 거리만큼 일정한 비율로 빛이 줄어든다.
    float travelled = max(distance - fogParams.x, 0.0f);
    float amount = 1.0f - exp(-travelled * fogParams.z);

    // 끝 거리에서는 반드시 완전히 덮는다. 지수 곡선만으로는 1 에 영영 닿지 않아
    // 무한 지형의 가장자리(아직 만들지 않은 청크)가 비쳐 보이기 때문이다.
    float edgeStart = lerp(fogParams.x, fogParams.y, 0.6f);
    amount = max(amount, smoothstep(edgeStart, fogParams.y, distance));

    // 내산란 : 해를 등지면 지평선 색, 해를 향하면 밝고 따뜻해진다.
    float3 toSun = normalize(-lightDir);
    float sunAmount = pow(saturate(dot(viewDir, toSun)), 24.0f) * fogParams.w;
    float3 fog = fogColor.rgb + float3(1.0f, 0.94f, 0.80f) * sunAmount;

    return lerp(rgb, fog, saturate(amount));
}

#endif
