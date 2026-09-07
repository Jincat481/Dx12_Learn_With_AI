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

    static const D3D11_INPUT_ELEMENT_DESC kLayout[3];
    static constexpr UINT kLayoutCount = 3;
};

// 터레인 상수 버퍼 (b0)
//  float4x4(64) + float4x4(64) + float4(16) + float4(16) = 160 bytes (16의 배수)
struct TerrainConstantBuffer
{
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4   color;
    DirectX::XMFLOAT4   params;   // x,y : 격자 칸 수  z : 와이어프레임(0/1)  w : 예약
};
