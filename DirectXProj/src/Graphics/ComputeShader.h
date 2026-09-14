#pragma once
#include "Core/stdafx.h"

// =============================================================
// ComputeShader (S71)
//  화면에 그리지 않고 GPU 에서 계산만 하는 셰이더.
//
//  VS/PS 는 정점과 픽셀이 들어오면 저절로 실행되지만,
//  컴퓨트 셰이더는 CPU 가 Dispatch(x, y, z) 로 "몇 묶음 돌려라" 하고 직접 부른다.
//
//      Dispatch(그룹 수) × [numthreads(스레드 수)] = 실제로 실행되는 횟수
//
//  결과는 보통 UAV(Unordered Access View) 로 붙인 텍스처나 버퍼에 쓴다.
//  cs_5_0 은 기능 수준 11_0 이상이 필요하다.
// =============================================================
class ComputeShader
{
public:
    bool Load(ID3D11Device* device, const std::wstring& path, const std::string& entry = "main");

    void Bind(ID3D11DeviceContext* context) const { context->CSSetShader(m_shader.Get(), nullptr, 0); }

    bool IsValid() const { return static_cast<bool>(m_shader); }
    const std::wstring& GetLastError() const { return m_lastError; }

private:
    ComPtr<ID3D11ComputeShader> m_shader;
    std::wstring m_lastError;
};
