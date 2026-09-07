#include "Core/stdafx.h"
#include "Graphics/Shader.h"
#include "Utils/StringUtil.h"

#include <filesystem>

namespace fs = std::filesystem;

// -------------------------------------------------------------
// HLSL 컴파일 플래그 (S23)
//  Debug : 디버그 정보 포함, 최적화 생략 → PIX/RenderDoc 에서 추적하기 쉽다
//  Release: 최고 최적화
// -------------------------------------------------------------
namespace
{
    UINT GetCompileFlags()
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
        return flags;
    }
}

std::wstring Shader::MakeCachePath(const std::wstring& hlslPath)
{
    fs::path path(hlslPath);
    path.replace_extension(L".cso");
    return path.wstring();
}

bool Shader::CompileHLSL(const std::wstring& path, const std::string& entry,
                         const std::string& profile, ComPtr<ID3DBlob>& outBlob)
{
    ComPtr<ID3DBlob> errorBlob;
    const HRESULT hr = ::D3DCompileFromFile(
        path.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,   // #include 를 파일 기준으로 해석한다
        entry.c_str(),
        profile.c_str(),
        GetCompileFlags(),
        0,
        outBlob.ReleaseAndGetAddressOf(),
        errorBlob.GetAddressOf());

    if (FAILED(hr))
    {
        // 컴파일 오류 메시지를 사람이 읽을 수 있게 남긴다.
        std::wstring message = L"셰이더 컴파일 실패 : " + path;
        if (errorBlob)
        {
            const char* text = static_cast<const char*>(errorBlob->GetBufferPointer());
            message += L"\n";
            message += StringUtil::Utf8ToWide(std::string(text, errorBlob->GetBufferSize()));
        }
        else
        {
            message += L" (파일을 찾을 수 없거나 읽을 수 없다)";
        }

        m_lastError = message;
        dxutil::DebugLog(L"[Shader] %s", message.c_str());
        return false;
    }

    return true;
}

void Shader::SaveBlobToCache(const std::wstring& hlslPath, ID3DBlob* blob) const
{
#if SHADER_CACHE_ENABLED
    if (!blob)
        return;

    const std::wstring cachePath = MakeCachePath(hlslPath);
    const HRESULT hr = ::D3DWriteBlobToFile(blob, cachePath.c_str(), TRUE /*overwrite*/);
    if (SUCCEEDED(hr))
        dxutil::DebugLog(L"[Shader] .cso 저장 : %s", cachePath.c_str());
    else
        dxutil::DebugLog(L"[Shader] .cso 저장 실패 : %s (0x%08X)", cachePath.c_str(), static_cast<unsigned>(hr));
#else
    (void)hlslPath; (void)blob;
#endif
}

// -------------------------------------------------------------
// 확장 과제 : .cso 캐시 로드 경로
//  .cso 가 존재하고 HLSL 보다 최신일 때만 사용한다.
// -------------------------------------------------------------
bool Shader::BuildFromCache(CompiledSet& out)
{
#if SHADER_CACHE_ENABLED
    if (!m_desc.useCache)
        return false;

    // 테셀레이션 셰이더까지 캐시하려면 경로가 4개로 늘어난다.
    // 학습용이라 그 경우는 그냥 매번 컴파일한다.
    if (!m_desc.hsPath.empty())
        return false;

    const std::wstring vsCache = MakeCachePath(m_desc.vsPath);
    const std::wstring psCache = MakeCachePath(m_desc.psPath);

    std::error_code ec;
    if (!fs::exists(vsCache, ec) || !fs::exists(psCache, ec))
        return false;

    // 원본 HLSL 이 더 최신이면 캐시는 낡은 것이다.
    const auto vsSrcTime = fs::last_write_time(m_desc.vsPath, ec);
    if (ec) return false;
    const auto psSrcTime = fs::last_write_time(m_desc.psPath, ec);
    if (ec) return false;

    if (fs::last_write_time(vsCache, ec) < vsSrcTime) return false;
    if (fs::last_write_time(psCache, ec) < psSrcTime) return false;

    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (FAILED(::D3DReadFileToBlob(vsCache.c_str(), vsBlob.GetAddressOf()))) return false;
    if (FAILED(::D3DReadFileToBlob(psCache.c_str(), psBlob.GetAddressOf()))) return false;

    if (DX_FAILED(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                               nullptr, out.vs.GetAddressOf()), L"CreateVertexShader(cache)"))
        return false;

    if (DX_FAILED(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                              nullptr, out.ps.GetAddressOf()), L"CreatePixelShader(cache)"))
        return false;

    if (DX_FAILED(m_device->CreateInputLayout(m_layoutElements.data(),
                                              static_cast<UINT>(m_layoutElements.size()),
                                              vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                              out.layout.GetAddressOf()), L"CreateInputLayout(cache)"))
        return false;

    dxutil::DebugLog(L"[Shader] .cso 캐시에서 로드 : %s", vsCache.c_str());
    return true;
#else
    (void)out;
    return false;
#endif
}

bool Shader::BuildFromSource(CompiledSet& out)
{
    ComPtr<ID3DBlob> vsBlob;
    if (!CompileHLSL(m_desc.vsPath, m_desc.vsEntry, m_desc.vsProfile, vsBlob))
        return false;

    ComPtr<ID3DBlob> psBlob;
    if (!CompileHLSL(m_desc.psPath, m_desc.psEntry, m_desc.psProfile, psBlob))
        return false;

    if (DX_FAILED(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                               nullptr, out.vs.GetAddressOf()), L"CreateVertexShader"))
    {
        m_lastError = L"CreateVertexShader 실패";
        return false;
    }

    if (DX_FAILED(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                              nullptr, out.ps.GetAddressOf()), L"CreatePixelShader"))
    {
        m_lastError = L"CreatePixelShader 실패";
        return false;
    }

    // Input Layout 은 VS 의 서명(signature)과 대조되어 검증된다. (S24)
    if (DX_FAILED(m_device->CreateInputLayout(m_layoutElements.data(),
                                              static_cast<UINT>(m_layoutElements.size()),
                                              vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                              out.layout.GetAddressOf()), L"CreateInputLayout"))
    {
        m_lastError = L"CreateInputLayout 실패 : Vertex 구조체와 HLSL semantic 이 일치하지 않는다";
        return false;
    }

    // ---- 선택 : Hull / Domain 셰이더 (S59) ----
    //  둘은 반드시 짝으로 있어야 한다. 하나만 있으면 파이프라인이 성립하지 않는다.
    if (!m_desc.hsPath.empty() && !m_desc.dsPath.empty())
    {
        ComPtr<ID3DBlob> hsBlob;
        if (!CompileHLSL(m_desc.hsPath, m_desc.hsEntry, m_desc.hsProfile, hsBlob))
            return false;

        ComPtr<ID3DBlob> dsBlob;
        if (!CompileHLSL(m_desc.dsPath, m_desc.dsEntry, m_desc.dsProfile, dsBlob))
            return false;

        if (DX_FAILED(m_device->CreateHullShader(hsBlob->GetBufferPointer(), hsBlob->GetBufferSize(),
                                                 nullptr, out.hs.GetAddressOf()), L"CreateHullShader"))
        {
            m_lastError = L"CreateHullShader 실패";
            return false;
        }

        if (DX_FAILED(m_device->CreateDomainShader(dsBlob->GetBufferPointer(), dsBlob->GetBufferSize(),
                                                   nullptr, out.ds.GetAddressOf()), L"CreateDomainShader"))
        {
            m_lastError = L"CreateDomainShader 실패";
            return false;
        }

        if (m_desc.cacheCompiled)
        {
            SaveBlobToCache(m_desc.hsPath, hsBlob.Get());
            SaveBlobToCache(m_desc.dsPath, dsBlob.Get());
        }
    }

    if (m_desc.cacheCompiled)
    {
        SaveBlobToCache(m_desc.vsPath, vsBlob.Get());
        SaveBlobToCache(m_desc.psPath, psBlob.Get());
    }

    return true;
}

bool Shader::Load(ID3D11Device* device, const Desc& desc)
{
    if (!device || !desc.layout || desc.layoutCount == 0)
        return false;

    m_device = device;
    m_desc = desc;

    m_layoutElements.assign(desc.layout, desc.layout + desc.layoutCount);
    m_desc.layout = m_layoutElements.data();

    return Reload();
}

bool Shader::Reload()
{
    if (!m_device)
        return false;

    m_lastError.clear();

    CompiledSet built;

    // 1) 캐시 경로를 먼저 시도하고, 실패하면 HLSL 을 컴파일한다.
    bool ok = BuildFromCache(built);
    if (!ok)
    {
        built = CompiledSet{};
        ok = BuildFromSource(built);
    }

    if (!ok)
    {
        // 실패 : 이전에 성공한 셰이더를 그대로 유지한다(화면이 깨지지 않는다).
        dxutil::DebugLog(L"[Shader] Reload 실패, 이전 셰이더를 유지한다.");
        return false;
    }

    // 2) 전부 성공했을 때만 교체(commit)한다.
    m_vertexShader = built.vs;
    m_pixelShader  = built.ps;
    m_inputLayout  = built.layout;
    m_hullShader   = built.hs;
    m_domainShader = built.ds;
    return true;
}

void Shader::Bind(ID3D11DeviceContext* context) const
{
    if (!context || !IsValid())
        return;

    context->IASetInputLayout(m_inputLayout.Get());
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // 테셀레이션을 쓰지 않는 셰이더로 넘어갈 때 이전 단계가 남지 않도록
    // 항상 명시적으로 설정한다(없으면 nullptr).
    context->HSSetShader(m_hullShader.Get(), nullptr, 0);
    context->DSSetShader(m_domainShader.Get(), nullptr, 0);
}
