#pragma once
#include "Core/stdafx.h"

// =============================================================
// Texture (과제 2 / S09, S11)
//  WIC 로 이미지 파일을 디코딩해 32bpp RGBA 로 변환한 뒤
//  ID3D11Texture2D 와 Shader Resource View 를 만든다.
//  GPU 리소스는 ComPtr 로 수명을 관리한다.
// =============================================================
class Texture
{
public:
    Texture() = default;
    ~Texture() = default;

    bool LoadFromFile(ID3D11Device* device, const std::wstring& path);

    // 공유 WIC 팩토리를 해제한다. CoUninitialize() 보다 먼저 호출해야 한다.
    static void ShutdownWIC();

    // 이미지 파일이 없을 때도 프레임워크를 확인할 수 있도록 절차적 텍스처를 만든다.
    bool CreateCheckerboard(ID3D11Device* device, UINT width, UINT height, UINT cellSize,
                            DirectX::XMFLOAT4 colorA, DirectX::XMFLOAT4 colorB);

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
    UINT GetWidth()  const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const std::wstring& GetPath() const { return m_path; }
    bool IsValid() const { return m_srv != nullptr; }

private:
    bool CreateFromPixels(ID3D11Device* device, const uint8_t* pixels, UINT width, UINT height, UINT rowPitch);

    ComPtr<ID3D11ShaderResourceView> m_srv;
    UINT         m_width = 0;
    UINT         m_height = 0;
    std::wstring m_path;
};
