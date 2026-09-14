// =============================================================
// TerrainCommon.hlsli - 터레인 VS / PS 공용 선언
//  C++ 의 TerrainConstantBuffer 와 멤버 순서·크기가 반드시 일치해야 한다.
//  VS 와 PS 가 각자 cbuffer 를 적어 두면 한쪽만 고치는 실수가 생기므로 한곳에 모았다.
// =============================================================
#ifndef TERRAIN_COMMON_HLSLI
#define TERRAIN_COMMON_HLSLI

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
    float4   gEyePos;        // xyz : 카메라 위치
    float4   gSurface;       // x : 트라이플래너(0/1)   y : 타일 한 장의 월드 크기   z : 블렌드 날카로움
    float4   gFogColor;      // rgb : 안개 색   a : 켜짐(0/1)
    float4   gFogParams;     // x : 시작 거리   y : 끝 거리   z : 밀도   w : 태양 산란 세기
    float4   gBrush;         // xy : 브러시 중심(월드 xz)   z : 반경   w : 도구 번호 + 1 (0 이면 끔)
};

#endif
