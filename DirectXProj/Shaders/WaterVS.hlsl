// =============================================================
// WaterVS.hlsl - 물 정점 셰이더 (S76)
//  물은 평평한 판 하나다. 물결은 정점을 흔들지 않고 픽셀 셰이더의 법선으로만 만든다.
//  판이 크고 정점이 4개뿐이라 정점을 움직여서는 잔물결을 표현할 수 없기 때문이다.
// =============================================================
#include "WaterCommon.hlsli"

struct VSInput
{
    float3 position : POSITION;
};

WaterPixel main(VSInput input)
{
    WaterPixel output;
    output.position = mul(float4(input.position, 1.0f), gWVP);
    output.worldPos = mul(float4(input.position, 1.0f), gWorld).xyz;
    return output;
}
