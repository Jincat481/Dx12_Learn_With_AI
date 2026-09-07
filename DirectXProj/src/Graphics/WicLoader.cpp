#include "Core/stdafx.h"
#include "Graphics/WicLoader.h"

#include <wincodec.h>

namespace wic
{
    namespace
    {
        // 주의 : 함수 지역 static ComPtr 로 두면 안 된다.
        //  CRT 의 정적 소멸은 CoUninitialize() 보다 뒤에 일어나므로,
        //  그때의 Release() 가 이미 내려간 COM 객체를 건드려 액세스 위반을 낸다.
        //  그래서 Shutdown() 으로 수명을 명시적으로 끊는다.
        ComPtr<IWICImagingFactory> g_factory;
    }

    IWICImagingFactory* GetFactory()
    {
        if (!g_factory)
        {
            const HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                                  IID_PPV_ARGS(g_factory.GetAddressOf()));
            if (!DX_CHECK(hr, L"CoCreateInstance(WICImagingFactory)"))
                return nullptr;
        }
        return g_factory.Get();
    }

    void Shutdown()
    {
        g_factory.Reset();
    }

    bool LoadPixelsRGBA(const std::wstring& path,
                        std::vector<uint8_t>& outPixels,
                        UINT& outWidth,
                        UINT& outHeight)
    {
        IWICImagingFactory* factory = GetFactory();
        if (!factory)
            return false;

        // 1) 디코더
        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
        if (FAILED(hr))
        {
            dxutil::DebugLog(L"[WIC] 이미지를 열 수 없다 : %s (0x%08X)", path.c_str(), static_cast<unsigned>(hr));
            return false;
        }

        // 2) 첫 프레임
        ComPtr<IWICBitmapFrameDecode> frame;
        if (DX_FAILED(decoder->GetFrame(0, frame.GetAddressOf()), L"IWICBitmapDecoder::GetFrame"))
            return false;

        // 3) 32bpp RGBA 로 통일
        ComPtr<IWICFormatConverter> converter;
        if (DX_FAILED(factory->CreateFormatConverter(converter.GetAddressOf()), L"CreateFormatConverter"))
            return false;

        if (DX_FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                            WICBitmapDitherTypeNone, nullptr, 0.0,
                                            WICBitmapPaletteTypeCustom), L"IWICFormatConverter::Initialize"))
            return false;

        if (DX_FAILED(converter->GetSize(&outWidth, &outHeight), L"IWICFormatConverter::GetSize"))
            return false;

        // 4) CPU 메모리로 복사. stride = 가로 픽셀 수 * 4바이트
        const UINT rowPitch = outWidth * 4;
        const UINT imageSize = rowPitch * outHeight;

        outPixels.resize(imageSize);
        if (DX_FAILED(converter->CopyPixels(nullptr, rowPitch, imageSize, outPixels.data()),
                      L"IWICFormatConverter::CopyPixels"))
            return false;

        return true;
    }
}
