// =============================================================
// TessCommon.hlsli - 테셀레이션 지형 공용 선언 (스텝 7)
// =============================================================

cbuffer TessConstants : register(b0)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4   gEyePosition;    // xyz : 카메라 위치
    float4   gTess;           // x 최소분할  y 최대분할  z 거리기준  w 높이배율
    float4   gLightDir;       // xyz 방향   w 환경광
    float4   gHeightRange;    // x 최저   y 최고
    float4   gParams;         // x 와이어프레임   y 지형 크기(월드)   z 텍셀 크기
};

// 높이맵 : CPU 의 노이즈를 한 번 구워 올린 텍스처.
//  도메인 셰이더가 새로 만든 정점의 높이를 여기서 읽는다.
Texture2D    gHeightMap : register(t0);
SamplerState gHeightSampler : register(s0);

struct ControlPoint
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
};

struct PatchConstants
{
    float edges[4]  : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

struct DomainOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};
