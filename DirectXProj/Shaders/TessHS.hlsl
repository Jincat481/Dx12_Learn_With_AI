// =============================================================
// TessHS.hlsl - 헐(Hull) 셰이더 : 얼마나 잘게 쪼갤지 정한다 (S60)
//
//  패치 하나마다 한 번 실행되는 "패치 상수 함수" 에서
//  네 변과 내부의 분할 계수를 정한다.
//
//  변의 계수는 **이웃 패치와 반드시 같아야 한다.**
//  그래서 변의 중점까지의 거리로 계산한다. 두 패치가 공유하는 변은
//  중점도 같으므로 양쪽이 같은 값을 얻어 틈이 생기지 않는다.
// =============================================================
#include "TessCommon.hlsli"

// 거리에 따라 분할 계수를 정한다. 가까우면 촘촘, 멀면 성기게.
float EdgeFactor(float3 a, float3 b)
{
    float3 midpoint = (a + b) * 0.5f;
    float distance = length(midpoint - gEyePosition.xyz);

    float t = saturate(distance / max(gTess.z, 0.001f));
    return lerp(gTess.y, gTess.x, t);   // 가까울수록 최대, 멀수록 최소
}

PatchConstants CalcPatchConstants(InputPatch<ControlPoint, 4> patch)
{
    PatchConstants output;

    // 제어점 순서 : 0 = 좌상, 1 = 우상, 2 = 좌하, 3 = 우하
    float3 p0 = mul(float4(patch[0].position, 1.0f), gWorld).xyz;
    float3 p1 = mul(float4(patch[1].position, 1.0f), gWorld).xyz;
    float3 p2 = mul(float4(patch[2].position, 1.0f), gWorld).xyz;
    float3 p3 = mul(float4(patch[3].position, 1.0f), gWorld).xyz;

    // 쿼드 도메인의 변 순서 : u=0, v=0, u=1, v=1
    output.edges[0] = EdgeFactor(p0, p2);   // 왼쪽
    output.edges[1] = EdgeFactor(p0, p1);   // 위
    output.edges[2] = EdgeFactor(p1, p3);   // 오른쪽
    output.edges[3] = EdgeFactor(p2, p3);   // 아래

    // 내부는 변들의 평균으로 둔다.
    float inside = (output.edges[0] + output.edges[1] + output.edges[2] + output.edges[3]) * 0.25f;
    output.inside[0] = inside;
    output.inside[1] = inside;

    return output;
}

[domain("quad")]
[partitioning("fractional_even")]     // 계수가 서서히 변해도 튀지 않는다
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("CalcPatchConstants")]
ControlPoint main(InputPatch<ControlPoint, 4> patch, uint id : SV_OutputControlPointID)
{
    return patch[id];
}
