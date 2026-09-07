#include "Core/stdafx.h"
#include "Graphics/Texture.h"

#include "Graphics/WicLoader.h"

using namespace DirectX;

bool Texture::LoadFromFile(ID3D11Device* device, const std::wstring& path)
{
    if (!device)
        return false;

    std::vector<uint8_t> pixels;
    UINT width = 0;
    UINT height = 0;

    if (!wic::LoadPixelsRGBA(path, pixels, width, height))
        return false;

    if (!CreateFromPixels(device, pixels.data(), width, height, width * 4))
        return false;

    m_path = path;
    dxutil::DebugLog(L"[Texture] 로드 완료 : %s (%ux%u)", path.c_str(), width, height);
    return true;
}

void Texture::ShutdownWIC()
{
    // CoUninitialize() 보다 반드시 먼저 호출되어야 한다.
    wic::Shutdown();
}

bool Texture::CreateCheckerboard(ID3D11Device* device, UINT width, UINT height, UINT cellSize,
                                 XMFLOAT4 colorA, XMFLOAT4 colorB)
{
    if (!device || width == 0 || height == 0)
        return false;

    if (cellSize == 0)
        cellSize = 8;

    const UINT rowPitch = width * 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(rowPitch) * height);

    auto toByte = [](float v) -> uint8_t
    {
        v = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
        return static_cast<uint8_t>(v * 255.0f + 0.5f);
    };

    for (UINT y = 0; y < height; ++y)
    {
        for (UINT x = 0; x < width; ++x)
        {
            const bool useA = (((x / cellSize) + (y / cellSize)) % 2) == 0;
            const XMFLOAT4& c = useA ? colorA : colorB;

            uint8_t* pixel = &pixels[static_cast<size_t>(y) * rowPitch + static_cast<size_t>(x) * 4];
            pixel[0] = toByte(c.x);
            pixel[1] = toByte(c.y);
            pixel[2] = toByte(c.z);
            pixel[3] = toByte(c.w);
        }
    }

    if (!CreateFromPixels(device, pixels.data(), width, height, rowPitch))
        return false;

    m_path = L"<checkerboard>";
    return true;
}

bool Texture::CreateFromPixels(ID3D11Device* device, const uint8_t* pixels, UINT width, UINT height, UINT rowPitch)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;   // 만든 뒤 바꾸지 않는다.
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem     = pixels;
    initData.SysMemPitch = rowPitch;

    ComPtr<ID3D11Texture2D> texture;
    if (DX_FAILED(device->CreateTexture2D(&desc, &initData, texture.GetAddressOf()), L"CreateTexture2D"))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format              = desc.Format;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (DX_FAILED(device->CreateShaderResourceView(texture.Get(), &srvDesc, m_srv.ReleaseAndGetAddressOf()),
                  L"CreateShaderResourceView"))
        return false;

    m_width = width;
    m_height = height;
    return true;
}
