#pragma once
#include "Core/stdafx.h"

// =============================================================
// Shader (과제 6 / S23, S24, S25)
//  HLSL 을 런타임에 D3DCompileFromFile 로 컴파일해
//  VertexShader / PixelShader / InputLayout 을 만든다.
//
//  Reload 는 "원자적"이다.
//   - 임시 변수에 모든 리소스를 만들고
//   - 전부 성공했을 때만 멤버로 교체(commit)한다.
//   - 하나라도 실패하면 마지막으로 성공한 셰이더를 그대로 쓴다.
// =============================================================
class Shader
{
public:
    struct Desc
    {
        std::wstring vsPath;
        std::wstring psPath;
        std::string  vsEntry = "main";
        std::string  psEntry = "main";
        std::string  vsProfile = "vs_5_0";
        std::string  psProfile = "ps_5_0";

        // 테셀레이션용. 비워 두면 쓰지 않는다. (스텝 7 / S59)
        //  Hull   : 패치를 얼마나 잘게 쪼갤지 정한다
        //  Domain : 쪼개진 각 점의 실제 위치를 만든다
        std::wstring hsPath;
        std::wstring dsPath;
        std::string  hsEntry = "main";
        std::string  dsEntry = "main";
        std::string  hsProfile = "hs_5_0";
        std::string  dsProfile = "ds_5_0";

        const D3D11_INPUT_ELEMENT_DESC* layout = nullptr;
        UINT layoutCount = 0;

        bool cacheCompiled = false;   // 컴파일 결과 Blob 을 .cso 로 저장한다
        bool useCache = false;        // .cso 가 최신이면 컴파일 대신 .cso 를 읽는다(확장 과제)
    };

    Shader() = default;
    ~Shader() = default;

    bool Load(ID3D11Device* device, const Desc& desc);
    bool Reload();                       // 실패해도 기존 셰이더를 유지한다

    void Bind(ID3D11DeviceContext* context) const;

    bool IsValid() const { return m_vertexShader && m_pixelShader && m_inputLayout; }
    bool HasTessellation() const { return m_hullShader && m_domainShader; }
    const Desc& GetDesc() const { return m_desc; }
    const std::wstring& GetLastError() const { return m_lastError; }

private:
    struct CompiledSet
    {
        ComPtr<ID3D11VertexShader> vs;
        ComPtr<ID3D11PixelShader>  ps;
        ComPtr<ID3D11InputLayout>  layout;
        ComPtr<ID3D11HullShader>   hs;
        ComPtr<ID3D11DomainShader> ds;
    };

    bool BuildFromSource(CompiledSet& out);                                   // HLSL 컴파일 경로
    bool BuildFromCache(CompiledSet& out);                                    // .cso 로드 경로
    bool CompileHLSL(const std::wstring& path, const std::string& entry,
                     const std::string& profile, ComPtr<ID3DBlob>& outBlob);
    static std::wstring MakeCachePath(const std::wstring& hlslPath);
    void SaveBlobToCache(const std::wstring& hlslPath, ID3DBlob* blob) const;

    ID3D11Device* m_device = nullptr;
    Desc          m_desc;
    std::vector<D3D11_INPUT_ELEMENT_DESC> m_layoutElements;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader>  m_pixelShader;
    ComPtr<ID3D11InputLayout>  m_inputLayout;
    ComPtr<ID3D11HullShader>   m_hullShader;
    ComPtr<ID3D11DomainShader> m_domainShader;

    std::wstring m_lastError;
};
