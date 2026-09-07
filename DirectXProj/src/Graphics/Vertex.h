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
