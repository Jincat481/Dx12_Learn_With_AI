// =============================================================
// SpritePS.hlsl - 스프라이트 픽셀 셰이더 (과제 2)
//  t0 : 텍스처, s0 : 샘플러
//  이 파일을 저장하면 Debug 빌드에서 Hot Reload 로 즉시 반영된다. (과제 6)
// =============================================================

cbuffer SpriteConstants : register(b0)
{
    float4x4 gWVP;
    float4   gColor;
    float4   gParams;   // x : 1 이면 텍스처 알파만 쓰는 단색 실루엣(선택 테두리)
};

Texture2D    gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 texel = gTexture.Sample(gSampler, input.uv);

    // 완전히 투명한 픽셀은 버린다.
    clip(texel.a - 0.01f);

    // 선택 테두리 : 텍스처의 모양(알파)만 쓰고 색은 gColor 단색으로 채운다.
    // 같은 Quad 를 조금 크게 그린 뒤 그 위에 원본을 겹치면 외곽선이 된다.
    if (gParams.x > 0.5f)
        return float4(gColor.rgb, texel.a * gColor.a);

    return texel * gColor;
}
