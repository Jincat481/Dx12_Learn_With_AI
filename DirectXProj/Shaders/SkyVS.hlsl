// =============================================================
// SkyVS.hlsl - 하늘 돔 정점 셰이더 (스텝 8 / S54)
// =============================================================

cbuffer SkyConstants : register(b0)
{
    float4x4 gWVP;
    float4   gHorizonColor;
    float4   gZenithColor;
    float4   gSunDirection;   // xyz : 빛이 나아가는 방향
    float4   gParams;         // x : 시간   y : 구름 사용   z : 구름 양   w : 구름 속도
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;     // 돔 중심에서의 방향을 담아 두었다
    float2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float4 position  : SV_POSITION;
    float3 direction : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    output.position = mul(float4(input.position, 1.0f), gWVP);

    // 깊이를 가장 먼 값(1.0)으로 밀어 둔다. (S55)
    //  원근 나눗셈 뒤 z/w 가 깊이가 되므로 z 에 w 를 넣으면 z/w = 1 이 된다.
    //  하늘은 깊이를 쓰지 않으므로, 이미 그려진 픽셀은 덮지 않고 배경만 채운다.
    output.position.z = output.position.w;

    // 색은 위치가 아니라 "어느 방향을 보고 있는가" 로 정한다.
    output.direction = normalize(input.normal);

    return output;
}
