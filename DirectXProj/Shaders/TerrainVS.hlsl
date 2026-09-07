// =============================================================
// TerrainVS.hlsl - 터레인 정점 셰이더 (스텝 1)
//  b0 : WVP / World / 색 / 파라미터
//  입력 semantic 은 C++ 의 TerrainVertex 구조체와 일치해야 한다. (S31)
// =============================================================

cbuffer TerrainConstants : register(b0)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4   gColor;
    float4   gParams;   // x,y : 격자 칸 수   z : 와이어프레임(0/1)
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // CPU 에서 전치해 넘겼으므로 행 벡터 * 행렬 순서로 곱한다.
    output.position = mul(float4(input.position, 1.0f), gWVP);
    output.worldPos = mul(float4(input.position, 1.0f), gWorld).xyz;

    // 법선은 위치가 아니라 방향이다. 이동 성분(4행)을 빼고 3x3 만 곱한다.
    output.normal = normalize(mul(input.normal, (float3x3)gWorld));

    output.uv = input.uv;
    return output;
}
