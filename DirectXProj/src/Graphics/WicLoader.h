#pragma once
#include "Core/stdafx.h"

// COM 인터페이스 전방 선언. 헤더에서 <wincodec.h> 를 끌어오지 않는다.
struct IWICImagingFactory;

// =============================================================
// WicLoader
//  WIC 팩토리와 "이미지 → CPU 픽셀 배열" 로딩을 한곳에 모았다.
//   - Texture     : 픽셀을 GPU 텍스처로 올린다
//   - HeightMapImage : 픽셀을 높이 값으로 읽는다
//  두 쓰임새가 같은 팩토리를 공유한다.
//
//  주의 : Shutdown 은 CoUninitialize 보다 먼저 호출되어야 한다.
// =============================================================
namespace wic
{
    IWICImagingFactory* GetFactory();

    // 어떤 포맷이든 32bpp RGBA 로 변환해 CPU 메모리에 담는다.
    bool LoadPixelsRGBA(const std::wstring& path,
                        std::vector<uint8_t>& outPixels,
                        UINT& outWidth,
                        UINT& outHeight);

    void Shutdown();
}
