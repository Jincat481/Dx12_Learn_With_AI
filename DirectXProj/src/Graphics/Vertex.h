#pragma once
#include "Core/stdafx.h"

// =============================================================
// 정점 구조체 (S10, S24)
//  이 구조체의 메모리 배치는 SpriteVS.hlsl 의 semantic 과 반드시 일치해야 한다.
//      offset  0 : POSITION (R32G32B32_FLOAT)
//      offset 12 : TEXCOORD (R32G32_FLOAT)
// =============================================================
struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 uv;

    static const D3D11_INPUT_ELEMENT_DESC kLayout[2];
    static constexpr UINT kLayoutCount = 2;
};

// 스프라이트 상수 버퍼 (b0)
//  HLSL 의 16바이트 정렬 규칙을 지킨다 : float4x4(64) + float4(16) + float4(16) = 96 bytes
struct SpriteConstantBuffer
{
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4   color;
    DirectX::XMFLOAT4   params;   // x : 실루엣 모드(0=일반, 1=단색 실루엣), y~w 예약
};

// =============================================================
// 터레인 정점 (S31)
//  TerrainVS.hlsl 의 semantic 과 메모리 배치가 일치해야 한다.
//      offset  0 : POSITION (R32G32B32_FLOAT)
//      offset 12 : NORMAL   (R32G32B32_FLOAT)
//      offset 24 : TEXCOORD (R32G32_FLOAT)
//      offset 32 : TEXCOORD1 (R32G32B32A32_FLOAT) 모프 타깃
//      offset 48 : TEXCOORD2 (R32G32B32A32_FLOAT) 바이옴 가중치
//  stride = 64 bytes
// =============================================================
struct TerrainVertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 uv;

    // 지오모핑용 모프 타깃 (S53)
    //  이 정점이 LOD 1 / 2 / 3 / 4 에서 가졌을 높이를 미리 계산해 둔다.
    //  셰이더에서 현재 LOD 에 해당하는 값 하나를 골라 지금 높이와 섞는다.
    DirectX::XMFLOAT4 morphTargets;

    // 바이옴 가중치 (S73) : 사막 / 초원 / 숲 / 설원. 합이 1 이다.
    //  CPU 가 기후 노이즈로 정점마다 계산해 넘긴다. 셰이더에 같은 노이즈를 또 짜지 않아도
    //  CPU 의 높이 식과 GPU 의 색이 항상 같은 바이옴을 가리킨다.
    DirectX::XMFLOAT4 biome;

    static const D3D11_INPUT_ELEMENT_DESC kLayout[5];
    static constexpr UINT kLayoutCount = 5;
};

// 터레인 상수 버퍼 (b0) - Shaders/TerrainCommon.hlsli 와 순서가 같아야 한다
//  float4x4 2개(128) + float4 12개(192) = 320 bytes (16의 배수)
struct TerrainConstantBuffer
{
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4   color;         // 평면 모드에서 쓰는 기본 색
    DirectX::XMFLOAT4   params;        // x,y : 격자 칸 수  z : 와이어프레임  w : 높이 사용(0/1)
    DirectX::XMFLOAT4   heightRange;   // x : 최저 높이  y : 최고 높이
    DirectX::XMFLOAT4   lightDirection;// xyz : 방향광이 나아가는 방향  w : 환경광 세기
    DirectX::XMFLOAT4   splat;         // x : 타일 반복  y : 스플래팅  z : 디버그 단색  w : 모프 계수
    DirectX::XMFLOAT4   lodSelect;     // 현재 LOD 에 해당하는 성분만 1 (모프 타깃 선택용)
    DirectX::XMFLOAT4   eyePosition;   // xyz : 카메라 위치 (안개 거리 계산)
    DirectX::XMFLOAT4   surface;       // x : 트라이플래너  y : 타일 한 장의 월드 크기  z : 블렌드 날카로움
    DirectX::XMFLOAT4   fogColor;      // rgb : 안개 색  a : 켜짐
    DirectX::XMFLOAT4   fogParams;     // x : 시작 거리  y : 끝 거리  z : 밀도  w : 태양 산란
    DirectX::XMFLOAT4   brush;         // xy : 브러시 중심(월드 xz)  z : 반경  w : 도구 번호 + 1 (0 이면 끔)
    DirectX::XMFLOAT4   clipPlane;     // 이 평면 아래는 잘라 낸다 (반사 패스, S76). 기본 (0,0,0,1) 은 아무것도 자르지 않는다
};

// 물 상수 버퍼 (b0) - Shaders/WaterCommon.hlsli 와 순서가 같아야 한다
//  float4x4 2개(128) + float4 13개(208) = 336 bytes
struct WaterConstantBuffer
{
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4   eyePosition;   // xyz 카메라 / w 시간
    DirectX::XMFLOAT4   shallowColor;
    DirectX::XMFLOAT4   deepColor;     // rgb / a 완전히 깊어지는 두께
    DirectX::XMFLOAT4   wave;          // x 파장 배율 / y 법선 세기 / z 속도 / w 굴절 왜곡
    DirectX::XMFLOAT4   lightDirection;
    DirectX::XMFLOAT4   projection;    // x near / y far / z 1/폭 / w 1/높이
    DirectX::XMFLOAT4   fogColor;
    DirectX::XMFLOAT4   fogParams;
    DirectX::XMFLOAT4   flags;         // x 반사 / y 굴절 / z 거품
    DirectX::XMFLOAT4   ripple;        // xy 물결 영역 원점 / z 크기 / w 법선 세기 (S79)
    DirectX::XMFLOAT4   ocean;         // xyz 파도 타일 3단 크기 / w 클릭 물결 텍셀 크기 (S85)
    DirectX::XMFLOAT4   ocean2;        // x 유의파고 / y 클릭 물결 높이 배율 / z 파도가 살아나는 깊이 / w 흰 파도머리 세기
    DirectX::XMFLOAT4   shore;         // xy 해안 마스크 원점 / z 크기 / w 있음 (S81)
};

// 폭포 상수 버퍼 (b0) - Shaders/Waterfall.hlsl 과 순서가 같아야 한다 (S90)
//  float4x4 2개(128) + float4 10개(160) = 288 bytes
struct WaterfallConstantBuffer
{
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4   eyePosition;    // xyz 카메라 / w 시간
    DirectX::XMFLOAT4   waterColor;     // rgb 물빛 / a 기본 불투명도
    DirectX::XMFLOAT4   foamColor;      // rgb 흰 거품 / a 거품 세기
    DirectX::XMFLOAT4   lightDirection;
    DirectX::XMFLOAT4   projection;     // x near / y far / z 1/폭 / w 1/높이
    DirectX::XMFLOAT4   fogColor;
    DirectX::XMFLOAT4   fogParams;
    DirectX::XMFLOAT4   params;         // x 무늬 흐름 속도 / y 부서짐 / z 예비 / w 디버그 색
    DirectX::XMFLOAT4   camRight;       // 물보라 빌보드용 카메라 축
    DirectX::XMFLOAT4   camUp;
};

// 테셀레이션 지형 상수 버퍼 (b0) : 64 * 2 + 16 * 7 = 240 bytes
struct TessConstantBuffer
{
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4   eyePosition;
    DirectX::XMFLOAT4   tess;          // x 최소분할 / y 최대분할 / z 거리기준 / w 높이배율
    DirectX::XMFLOAT4   lightDirection;
    DirectX::XMFLOAT4   heightRange;
    DirectX::XMFLOAT4   params;        // x 와이어프레임 / y 지형 크기 / z 텍셀 크기
    DirectX::XMFLOAT4   fogColor;      // rgb 안개 색 / a 켜짐
    DirectX::XMFLOAT4   fogParams;     // x 시작 / y 끝 / z 밀도 / w 태양 산란
};

// 물결 텍스처 보기 상수 버퍼 (b0) - Shaders/RippleView.hlsl : float4 4개 = 64 bytes (S82)
struct RippleViewConstantBuffer
{
    DirectX::XMFLOAT4 region;
    DirectX::XMFLOAT4 shore;
    DirectX::XMFLOAT4 marker;
    DirectX::XMFLOAT4 params;
};

// 하늘 상수 버퍼 (b0) : 64 + 16 * 4 = 128 bytes
struct SkyConstantBuffer
{
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4   horizonColor;
    DirectX::XMFLOAT4   zenithColor;
    DirectX::XMFLOAT4   sunDirection;
    DirectX::XMFLOAT4   params;        // x 시간 / y 구름 사용 / z 구름 양 / w 구름 속도
};
