// =============================================================
// WaterCommon.hlsli - 물 VS / PS 공용 선언 (S76~S81, S85~S87)
//  C++ 의 WaterConstantBuffer 와 멤버 순서·크기가 일치해야 한다.
// =============================================================
#ifndef WATER_COMMON_HLSLI
#define WATER_COMMON_HLSLI

cbuffer WaterConstants : register(b0)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4   gEyePos;        // xyz : 카메라 위치   w : 시간(초)
    float4   gShallowColor;  // rgb : 얕은 물 색
    float4   gDeepColor;     // rgb : 깊은 바다 색   a : 이만큼 깊으면 바닥이 완전히 안 보인다
    float4   gWave;          // w : 굴절 왜곡
    float4   gLightDir;      // xyz : 햇빛이 나아가는 방향
    float4   gProjection;    // x : near   y : far   z : 1/화면폭   w : 1/화면높이
    float4   gFogColor;      // rgb : 안개(지평선) 색   a : 켜짐
    float4   gFogParams;     // x 시작 y 끝 z 밀도 w 태양 산란
    float4   gFlags;         // x : 반사   y : 굴절·깊이   z : 거품   w : 물결 디버그 색
    float4   gRipple;        // xy : 클릭 물결 영역 원점(월드 xz)   z : 한 변 길이   w : 물결 법선 세기 (0 이면 끔)
    float4   gOcean;         // xyz : 파도 타일 3단의 크기(m)   w : 클릭 물결 텍셀 크기(UV)
    float4   gOcean2;        // x : 유의파고(m)   y : 클릭 물결 높이 배율   z : 파도가 다 살아나는 깊이(m)   w : 흰 파도머리 세기
    float4   gShore;         // xy : 해안 높이 맵 원점(월드 xz)   z : 한 변 길이   w : 있음 (S81)
};

struct WaterPixel
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;    // 파도로 움직인 뒤의 위치
    float4 ocean    : TEXCOORD0;   // xy : 움직이기 전 수면 위치(xz)   z : 해안 잠잠함(0~1)   w : 파도 높이(m)
};

#endif
