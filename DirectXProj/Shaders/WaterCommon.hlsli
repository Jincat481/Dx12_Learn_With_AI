// =============================================================
// WaterCommon.hlsli - 물 VS / PS 공용 선언 (S76~S78)
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
    float4   gDeepColor;     // rgb : 깊은 물 색   a : 이만큼 깊으면 바닥이 완전히 안 보인다
    float4   gWave;          // x : 파장 배율   y : 법선 세기   z : 흐르는 속도   w : 굴절 왜곡
    float4   gLightDir;      // xyz : 햇빛이 나아가는 방향
    float4   gProjection;    // x : near   y : far   z : 1/화면폭   w : 1/화면높이
    float4   gFogColor;      // rgb : 안개(지평선) 색   a : 켜짐
    float4   gFogParams;     // x 시작 y 끝 z 밀도 w 태양 산란
    float4   gFlags;         // x : 반사   y : 굴절·깊이   z : 거품
    float4   gPadding;
};

struct WaterPixel
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
};

#endif
