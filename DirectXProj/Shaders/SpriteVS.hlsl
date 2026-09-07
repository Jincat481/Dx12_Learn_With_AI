// =============================================================
// SpriteVS.hlsl - 스프라이트 정점 셰이더 (과제 2 / S12)
//  b0 : WVP 행렬 + 색상 틴트
//  입력 semantic 은 C++ 의 Vertex 구조체와 일치해야 한다. (S24)
// =============================================================

cbuffer SpriteConstants : register(b0)
{
    float4x4 gWVP;
    float4   gColor;
    float4   gParams;   // x : 1 이면 텍스처 알파만 쓰는 단색 실루엣(선택 테두리)
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // CPU 에서 전치해 넘겼으므로 행 벡터 * 행렬 순서로 곱한다.
    output.position = mul(float4(input.position, 1.0f), gWVP);
    output.uv       = input.uv;

    return output;
}
