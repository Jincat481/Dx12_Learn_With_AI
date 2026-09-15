// =============================================================
// WaterVS.hlsl - 바다 수면 정점 셰이더 (S85)
//
//  수면은 카메라를 따라다니는 촘촘한 격자다. 가운데는 0.3 m 간격, 멀어질수록 넓어진다.
//  정점마다 파도 변위 텍스처 3단을 읽어 실제로 들어 올리고 앞뒤로 민다.
//  (평면에 법선만 입히면 옆에서 볼 때 수평선이 일자로 보인다. 파도는 실제 모양이 있어야 한다)
//
//  - 멀리 있는 작은 파도는 화면에서 한 픽셀도 안 되므로 거리에 따라 뺀다 (깜빡임 방지)
//  - 섬 근처 얕은 물은 해안 높이 맵으로 깊이를 알아 파도를 잠재운다
//  - 클릭 물결 높이도 더한다
// =============================================================
#include "WaterCommon.hlsli"

Texture2D    gRippleHeight  : register(t3);
Texture2D    gShoreMask     : register(t4);
Texture2D    gDisplacement0 : register(t5);
Texture2D    gDisplacement1 : register(t6);
Texture2D    gDisplacement2 : register(t7);
SamplerState gClamp : register(s0);
SamplerState gWrap  : register(s1);

struct VSInput
{
    float3 position : POSITION;
};

WaterPixel main(VSInput input)
{
    WaterPixel output;

    // 월드 행렬은 이동뿐이다 (격자를 카메라 발밑 수위에 둔다)
    float3 world = mul(float4(input.position, 1.0f), gWorld).xyz;
    float distance = length(world.xz - gEyePos.xz);

    // ---- 해안 : 얕을수록 잠잠하게 ----
    float calm = 1.0f;
    if (gShore.w > 0.5f)
    {
        float2 shoreUV = (world.xz - gShore.xy) / gShore.z;
        if (all(shoreUV >= 0.0f) && all(shoreUV <= 1.0f))
        {
            float elevation = gShoreMask.SampleLevel(gClamp, shoreUV, 0).r;   // 땅 높이 - 수위
            if (elevation > -999.0f)
                calm = lerp(0.12f, 1.0f, smoothstep(0.0f, gOcean2.z, -elevation));
        }
    }

    // ---- 파도 3단 ----
    float fade0 = 1.0f - smoothstep(700.0f, 1400.0f, distance);
    float fade1 = 1.0f - smoothstep(250.0f, 550.0f, distance);
    float fade2 = 1.0f - smoothstep(60.0f, 150.0f, distance);

    float3 displacement =
        gDisplacement0.SampleLevel(gWrap, world.xz / gOcean.x, 0).xyz * fade0 +
        gDisplacement1.SampleLevel(gWrap, world.xz / gOcean.y, 0).xyz * fade1 +
        gDisplacement2.SampleLevel(gWrap, world.xz / gOcean.z, 0).xyz * fade2;
    displacement *= calm;

    // ---- 클릭 물결 ----
    if (gRipple.w > 0.0f)
    {
        float2 rippleUV = (world.xz - gRipple.xy) / gRipple.z;
        if (all(rippleUV >= 0.0f) && all(rippleUV <= 1.0f))
            displacement.y += gRippleHeight.SampleLevel(gClamp, rippleUV, 0).r * gOcean2.y;
    }

    output.position = mul(float4(input.position + displacement, 1.0f), gWVP);
    output.worldPos = world + displacement;
    output.ocean = float4(world.xz, calm, displacement.y);
    return output;
}
