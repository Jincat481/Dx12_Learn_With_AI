#include "Core/stdafx.h"
#include "Graphics/ComputeShader.h"
#include "Utils/StringUtil.h"

bool ComputeShader::Load(ID3D11Device* device, const std::wstring& path, const std::string& entry)
{
    m_shader.Reset();
    m_lastError.clear();

    if (!device)
    {
        m_lastError = L"디바이스가 없다";
        return false;
    }

    // 기능 수준 10_x 의 컴퓨트 셰이더(cs_4_x)는 쓸 수 있는 기능이 크게 제한된다.
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0)
    {
        m_lastError = L"cs_5_0 은 기능 수준 11_0 이상이 필요하다";
        return false;
    }

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = ::D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                            entry.c_str(), "cs_5_0", flags, 0,
                                            blob.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        m_lastError = L"컴퓨트 셰이더 컴파일 실패 : " + path;
        if (errors)
        {
            const char* text = static_cast<const char*>(errors->GetBufferPointer());
            m_lastError += L"\n" + StringUtil::Utf8ToWide(std::string(text, errors->GetBufferSize()));
        }
        dxutil::DebugLog(L"[ComputeShader] %s", m_lastError.c_str());
        return false;
    }

    if (DX_FAILED(device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                              nullptr, m_shader.GetAddressOf()),
                  L"CreateComputeShader"))
    {
        m_lastError = L"CreateComputeShader 실패";
        return false;
    }

    return true;
}
