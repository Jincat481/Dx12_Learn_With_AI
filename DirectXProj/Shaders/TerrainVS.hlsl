// =============================================================
// TerrainVS.hlsl - 터레인 정점 셰이더 (스텝 1)
//  b0 : WVP / World / 색 / 파라미터
//  입력 semantic 은 C++ 의 TerrainVertex 구조체와 일치해야 한다. (S31)
// =============================================================

cbuffer TerrainConstants : register(b0)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4   gColor;         // 평면 모드의 기본 색
    float4   gParams;        // x,y : 격자 칸 수   z : 와이어프레임   w : 높이 사용(0/1)
    float4   gHeightRange;   // x : 최저 높이   y : 최고 높이
    float4   gLightDir;      // xyz : 방향광이 나아가는 방향   w : 환경광 세기
    float4   gSplat;         // x : 타일 반복   y : 스플래팅   z : 디버그 단색   w : 모프 계수
    float4   gLodSelect;     // 현재 LOD 성분만 1
};

struct VSInput
{
    float3 position     : POSITION;
    float3 normal       : NORMAL;
    float2 uv           : TEXCOORD0;
    float4 morphTargets : TEXCOORD1;   // LOD 1~4 에서의 높이
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

    // ---- 지오모핑 (S53) ----
    //  LOD 가 바뀌는 순간 지형이 툭 튀는 것을 막는다.
    //  전환 거리에 가까워질수록 다음 LOD 의 모양으로 미리 서서히 옮겨 두면,
    //  실제로 인덱스를 바꿔 끼우는 순간에는 이미 모양이 같아 티가 나지 않는다.
    float3 position = input.position;

    float morphTarget = dot(input.morphTargets, gLodSelect);
    position.y = lerp(position.y, morphTarget, saturate(gSplat.w));

    // CPU 에서 전치해 넘겼으므로 행 벡터 * 행렬 순서로 곱한다.
    output.position = mul(float4(position, 1.0f), gWVP);
    output.worldPos = mul(float4(position, 1.0f), gWorld).xyz;

    // 법선은 위치가 아니라 방향이다. 이동 성분(4행)을 빼고 3x3 만 곱한다.
    output.normal = normalize(mul(input.normal, (float3x3)gWorld));

    output.uv = input.uv;
    return output;
}
