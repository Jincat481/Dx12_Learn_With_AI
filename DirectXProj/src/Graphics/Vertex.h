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
//  stride = 32 bytes
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

    static const D3D11_INPUT_ELEMENT_DESC kLayout[4];
    static constexpr UINT kLayoutCount = 4;
};

// 터레인 상수 버퍼 (b0)
//  float4x4(64) + float4x4(64) + float4(16) + float4(16) = 160 bytes (16의 배수)
//  64 + 64 + 16 * 5 = 208 bytes (16의 배수)
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
};
