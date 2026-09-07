// =============================================================
// TessDS.hlsl - 도메인(Domain) 셰이더 : 새 정점을 만든다 (S61)
//
//  헐 셰이더가 정한 만큼 쪼개진 뒤, 쪼개진 점마다 한 번씩 실행된다.
//  SV_DomainLocation 은 패치 안에서의 위치(0~1, 0~1)다.
//
//  여기서 하는 일
//   1) 네 제어점을 이중선형 보간해 평면 위 위치를 만든다
//   2) 높이맵에서 높이를 읽어 y 를 올린다 (변위 매핑)
//   3) 높이맵의 기울기로 법선을 만든다
//
//  높이가 CPU 함수가 아니라 텍스처인 이유가 여기 있다.
//  정점이 GPU 에서 생기므로 높이도 GPU 에서 읽을 수 있어야 한다.
// =============================================================
#include "TessCommon.hlsli"

float SampleHeight(float2 uv)
{
    return gHeightMap.SampleLevel(gHeightSampler, uv, 0).r * gTess.w;
}

[domain("quad")]
DomainOutput main(PatchConstants constants,
                  float2 domain : SV_DomainLocation,
                  const OutputPatch<ControlPoint, 4> patch)
{
    DomainOutput output;

    // 1) 이중선형 보간 : 위 두 점 사이, 아래 두 점 사이를 먼저 섞고 다시 섞는다.
    float3 top    = lerp(patch[0].position, patch[1].position, domain.x);
    float3 bottom = lerp(patch[2].position, patch[3].position, domain.x);
    float3 position = lerp(top, bottom, domain.y);

    float2 uvTop    = lerp(patch[0].uv, patch[1].uv, domain.x);
    float2 uvBottom = lerp(patch[2].uv, patch[3].uv, domain.x);
    float2 uv = lerp(uvTop, uvBottom, domain.y);

    // 2) 변위 매핑 : 높이맵에서 읽은 값만큼 위로 올린다.
    position.y = SampleHeight(uv);

    // 3) 법선 : 좌우/상하 한 텍셀씩 떨어진 높이의 차이(중앙 차분)
    float texel = gParams.z;
    float worldStep = gParams.y * texel;   // 텍셀 하나가 월드에서 차지하는 길이

    float hL = SampleHeight(uv - float2(texel, 0.0f));
    float hR = SampleHeight(uv + float2(texel, 0.0f));
    float hD = SampleHeight(uv - float2(0.0f, texel));
    float hU = SampleHeight(uv + float2(0.0f, texel));

    // uv.y 가 커지면 -Z 방향이므로 부호를 뒤집는다.
    float3 normal = normalize(float3(-(hR - hL) / (2.0f * worldStep),
                                     1.0f,
                                     (hU - hD) / (2.0f * worldStep)));

    output.worldPos = mul(float4(position, 1.0f), gWorld).xyz;
    output.position = mul(float4(position, 1.0f), gWVP);
    output.normal   = normalize(mul(normal, (float3x3)gWorld));
    output.uv       = uv;

    return output;
}
