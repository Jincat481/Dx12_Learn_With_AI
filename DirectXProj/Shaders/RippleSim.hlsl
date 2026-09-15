// =============================================================
// RippleSim.hlsl - 물결 퍼짐 시뮬레이션 (S79, S81)
//  렌더 타깃(RTV)에 그리는 방식으로 GPU 에서 계산한다.
//
//  1) "다음" 높이 텍스처를 렌더 타깃으로 건다
//  2) 화면 전체를 덮는 삼각형 하나를 그린다 → 텍셀마다 픽셀 셰이더가 한 번씩 돈다
//  3) 픽셀 셰이더가 "이전 · 현재" 텍스처를 읽어 다음 높이를 반환한다
//
//  2차원 파동 방정식  ∂²h/∂t² = c² ∇²h  를 이전 · 현재 · 다음 세 장으로 근사하면
//
//      다음 = (위 + 아래 + 왼쪽 + 오른쪽) / 2 - 이전
//
//  (c² · dt² / dx² = 1/2 로 고른 식이다. 더 크면 값이 폭발하고, 작으면 느리게 퍼진다.)
//
//  이 식은 선형이라 두 물결이 만나면 높이가 그대로 더해진다(중첩).
//  마루와 마루가 만나면 더 높아지고 마루와 골이 만나면 상쇄된다 — 이것이 간섭 무늬다.
//
//  해안 (S81) : 땅인 텍셀의 높이를 항상 0 으로 묶어 두면 그 자리가 "벽" 이 된다.
//  벽에 닿은 물결은 되돌아 나가고, 들어오는 물결과 만나 간섭한다.
// =============================================================

cbuffer RippleConstants : register(b0)
{
    int2   gShift;       // 영역이 카메라를 따라 옮겨졌을 때, 이전 결과를 읽을 텍셀 오프셋
    float  gDamping;     // 한 단계마다 곱하는 감쇠
    uint   gDropCount;   // 이번 단계에 떨어뜨릴 물방울 수 (최대 4)
    float4 gDrops[4];    // xy : 텍셀 좌표   z : 반경(텍셀)   w : 세기
    float4 gShore;       // 텍셀 → 해안 마스크 UV : uv = (텍셀 + 0.5) · x + (y, z)   w : 사용 여부
};

Texture2D<float> gPrevious  : register(t0);   // h(t - 1)
Texture2D<float> gCurrent   : register(t1);   // h(t)
Texture2D<float> gShoreMask : register(t2);   // 1 = 땅, 0 = 물
SamplerState     gLinear    : register(s0);

struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

// ---- 정점 셰이더 : 정점 버퍼 없이 번호(SV_VertexID)만으로 화면을 덮는 삼각형을 만든다 ----
//  0 → (-1,  1)   1 → ( 3,  1)   2 → (-1, -3)
//  화면(-1~1)보다 큰 삼각형 하나가 사각형 두 개보다 간단하고, 대각선 이음매도 없다.
FullscreenVertex VSMain(uint id : SV_VertexID)
{
    FullscreenVertex output;
    float2 corner = float2((id << 1) & 2, id & 2);
    output.position = float4(corner * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

// 영역 밖은 잔잔한 물(높이 0)로 본다. 가장자리에 닿은 물결은 흡수되듯 사라진다.
float LoadHeight(Texture2D<float> heights, int2 p)
{
    uint width, height;
    heights.GetDimensions(width, height);

    if (p.x < 0 || p.y < 0 || p.x >= (int)width || p.y >= (int)height)
        return 0.0f;

    return heights.Load(int3(p, 0));
}

bool IsLand(int2 texel)
{
    if (gShore.w < 0.5f)
        return false;

    float2 uv = (float2(texel) + 0.5f) * gShore.x + gShore.yz;
    if (any(uv < 0.0f) || any(uv > 1.0f))
        return false;

    // 해안 높이 맵(땅 높이 - 수위)을 선형 보간한 값이 0 을 넘는 곳이 땅이다. 해안선이 계단지지 않는다.
    return gShoreMask.SampleLevel(gLinear, uv, 0) > 0.0f;
}

// ---- 픽셀 셰이더 : 텍셀 하나의 다음 높이 ----
float PSMain(FullscreenVertex input) : SV_TARGET
{
    // SV_POSITION.xy 는 픽셀 중심(0.5, 1.5, ...)이다. 정수로 자르면 텍셀 번호가 된다.
    int2 texel = int2(input.position.xy);

    // 땅은 움직이지 않는 벽이다. 높이를 0 에 묶어 두면 물결이 여기서 반사된다.
    if (IsLand(texel))
        return 0.0f;

    // 영역이 옮겨졌다면 같은 월드 위치의 예전 텍셀을 읽는다.
    int2 p = texel + gShift;

    float neighbors = LoadHeight(gCurrent, p + int2( 1,  0)) +
                      LoadHeight(gCurrent, p + int2(-1,  0)) +
                      LoadHeight(gCurrent, p + int2( 0,  1)) +
                      LoadHeight(gCurrent, p + int2( 0, -1));

    float next = neighbors * 0.5f - LoadHeight(gPrevious, p);
    next *= gDamping;

    // 격자 잡음 감쇠 (S88)
    //  이 차분식은 텍셀마다 +/- 가 번갈아 뒤집히는 체크무늬(격자로 표현할 수 있는 가장 짧은 파장)를 거의 줄이지 못한다.
    //  세게 누르거나 빗방울이 쌓이면 그 무늬가 자라 상한(±8)에 걸리고, 수면에 체크무늬 얼룩이 생긴다.
    //  이웃 평균 쪽으로 조금 당기면 긴 물결은 거의 그대로이고 체크무늬만 빠르게 사라진다.
    //  (체크무늬는 이웃 평균이 자기 값의 정반대라 당기는 힘이 가장 크다)
    float neighborAverage = neighbors * 0.25f;
    next = lerp(next, neighborAverage, 0.02f);

    // 물방울 : 가우스 모양으로 수면을 눌러 준다. 눌린 자리가 되튀며 동심원이 퍼져 나간다.
    for (uint i = 0; i < gDropCount; ++i)
    {
        float2 offset = float2(texel) + 0.5f - gDrops[i].xy;
        float radius = max(gDrops[i].z, 0.5f);
        next -= gDrops[i].w * exp(-dot(offset, offset) / (radius * radius));
    }

    // 안전장치 : 너무 세게 연달아 누르면 값이 커질 수 있다.
    return clamp(next, -4.0f, 4.0f);
}
