#include "Core/stdafx.h"
#include "Graphics/Texture.h"

#include <wincodec.h>

using namespace DirectX;

namespace
{
    // WIC 팩토리는 프로세스에 하나만 두면 충분하다. (COM 초기화는 Game 에서 수행)
    //
    // 주의 : 이것을 함수 지역 static ComPtr 로 두면 안 된다.
    //   CRT 의 정적 소멸은 CoUninitialize() 보다 뒤에 일어나므로,
    //   그 시점의 Release() 는 이미 내려간 COM 객체를 건드려
    //   wrl/client.h 의 InternalRelease() 안에서 액세스 위반을 낸다.
    //   그래서 Texture::ShutdownWIC() 로 수명을 명시적으로 끊는다.
    ComPtr<IWICImagingFactory> g_wicFactory;

    IWICImagingFactory* GetWICFactory()
    {
        if (!g_wicFactory)
        {
            const HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                                  IID_PPV_ARGS(g_wicFactory.GetAddressOf()));
            if (!DX_CHECK(hr, L"CoCreateInstance(WICImagingFactory)"))
                return nullptr;
        }
        return g_wicFactory.Get();
    }
}

void Texture::ShutdownWIC()
{
    // CoUninitialize() 보다 반드시 먼저 호출되어야 한다.
    g_wicFactory.Reset();
}

bool Texture::LoadFromFile(ID3D11Device* device, const std::wstring& path)
{
    if (!device)
        return false;

    IWICImagingFactory* factory = GetWICFactory();
    if (!factory)
        return false;

    // 1) 디코더 생성
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(hr))
    {
        dxutil::DebugLog(L"[Texture] 이미지를 열 수 없다 : %s (0x%08X)", path.c_str(), static_cast<unsigned>(hr));
        return false;
    }

    // 2) 첫 프레임 가져오기
    ComPtr<IWICBitmapFrameDecode> frame;
    if (DX_FAILED(decoder->GetFrame(0, frame.GetAddressOf()), L"IWICBitmapDecoder::GetFrame"))
        return false;

    // 3) 포맷을 32bpp RGBA 로 통일한다(D3D 의 R8G8B8A8_UNORM 과 대응).
    ComPtr<IWICFormatConverter> converter;
    if (DX_FAILED(factory->CreateFormatConverter(converter.GetAddressOf()), L"CreateFormatConverter"))
        return false;

    if (DX_FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                        WICBitmapDitherTypeNone, nullptr, 0.0,
                                        WICBitmapPaletteTypeCustom), L"IWICFormatConverter::Initialize"))
        return false;

    UINT width = 0, height = 0;
    if (DX_FAILED(converter->GetSize(&width, &height), L"IWICFormatConverter::GetSize"))
        return false;

    // 4) CPU 메모리로 픽셀을 복사한다. stride = 가로 픽셀 수 * 4바이트
    const UINT rowPitch = width * 4;
    const UINT imageSize = rowPitch * height;

    std::vector<uint8_t> pixels(imageSize);
    if (DX_FAILED(converter->CopyPixels(nullptr, rowPitch, imageSize, pixels.data()), L"IWICFormatConverter::CopyPixels"))
        return false;

    if (!CreateFromPixels(device, pixels.data(), width, height, rowPitch))
        return false;

    m_path = path;
    dxutil::DebugLog(L"[Texture] 로드 완료 : %s (%ux%u)", path.c_str(), width, height);
    return true;
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
