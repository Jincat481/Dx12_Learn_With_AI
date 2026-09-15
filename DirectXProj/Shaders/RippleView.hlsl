// =============================================================
// RippleView.hlsl - 물결 높이 텍스처를 화면 한쪽에 그대로 보여 준다 (S82)
//
//  시뮬레이션이 렌더 타깃에 써 넣은 값(R32_FLOAT)을 색으로 바꿀 뿐이다.
//   빨강 : 수면보다 솟은 곳 (+)   파랑 : 꺼진 곳 (-)   진할수록 진폭이 크다
//   노랑 : 물결이 반사되는 해안 벽   흰 가로줄 : 아래 단면 그래프가 읽는 줄
//  위쪽이 +Z(북쪽)가 되도록 세로를 뒤집는다. 텍스처의 행 0 은 가장 남쪽이다.
// =============================================================

cbuffer RippleViewConstants : register(b0)
{
    float4 gRegion;    // xy : 물결 영역 원점(월드 xz)   z : 한 변 길이
    float4 gShore;     // xy : 해안 마스크 원점   z : 한 변 길이   w : 있음
    float4 gMarker;    // xy : 표시할 점(텍스처 UV)   z : 단면 줄(텍스처 V)   w : 있음
    float4 gParams;    // x : 이 진폭이면 색이 가득 찬다   y : 창 크기(픽셀)
};

Texture2D<float> gHeight    : register(t0);
Texture2D<float> gShoreMask : register(t1);
SamplerState     gSampler   : register(s0);

struct ViewVertex
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

// 정점 버퍼 없이 화면(여기서는 뷰포트로 좁힌 창)을 덮는 삼각형
ViewVertex VSMain(uint id : SV_VertexID)
{
    ViewVertex output;
    float2 corner = float2((id << 1) & 2, id & 2);
    output.position = float4(corner * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.uv = corner;
    return output;
}

float4 PSMain(ViewVertex input) : SV_TARGET
{
    float2 tex = float2(input.uv.x, 1.0f - input.uv.y);   // 북쪽이 위
    float height = gHeight.SampleLevel(gSampler, tex, 0);

    // 작은 진폭도 보이도록 제곱근 곡선을 씌운다 (0.01 → 0.1, 0.25 → 0.5)
    float amount = sqrt(saturate(abs(height) / max(gParams.x, 1e-4f)));

    const float3 background = float3(0.05f, 0.07f, 0.10f);
    const float3 crest      = float3(1.00f, 0.32f, 0.22f);
    const float3 trough     = float3(0.25f, 0.52f, 1.00f);

    float3 color = lerp(background, (height >= 0.0f) ? crest : trough, amount);

    // 해안 벽
    if (gShore.w > 0.5f)
    {
        float2 world = gRegion.xy + tex * gRegion.z;
        float2 shoreUV = (world - gShore.xy) / gShore.z;
        if (all(shoreUV >= 0.0f) && all(shoreUV <= 1.0f) && gShoreMask.SampleLevel(gSampler, shoreUV, 0) > 0.5f)
            color = lerp(color, float3(1.0f, 0.85f, 0.1f), 0.55f);
    }

    if (gMarker.w > 0.5f)
    {
        float pixel = 1.0f / max(gParams.y, 1.0f);

        // 단면 줄
        if (abs(tex.y - gMarker.z) < pixel)
            color = lerp(color, float3(1.0f, 1.0f, 1.0f), 0.55f);

        // 표시 점 : 반지름 4 픽셀 고리
        float distance = length(tex - gMarker.xy) / pixel;
        if (distance > 3.0f && distance < 5.0f)
            color = float3(1.0f, 1.0f, 1.0f);
    }

    return float4(color, 1.0f);
}
