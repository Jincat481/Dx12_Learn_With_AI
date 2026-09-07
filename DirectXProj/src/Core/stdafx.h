#pragma once
// =============================================================
// stdafx.h - 프로젝트 공통 헤더
//  - Win32 / D3D11 / DirectXMath 공통 include
//  - ComPtr(GPU 리소스), unique_ptr(엔진 소유 객체) 수명 규칙
//  - HOT_RELOAD_ENABLED : 과제 6의 조건부 컴파일 스위치 (S26)
// =============================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <wrl/client.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

// -------------------------------------------------------------
// Hot Reload 스위치
//  Debug 빌드에서만 파일 감시 코드가 컴파일된다.
//  Release 빌드에서는 std::filesystem 검사 코드 자체가 제외된다.
// -------------------------------------------------------------
#ifndef HOT_RELOAD_ENABLED
    #ifdef _DEBUG
        #define HOT_RELOAD_ENABLED 1
    #else
        #define HOT_RELOAD_ENABLED 0
    #endif
#endif

// 셰이더 컴파일 결과를 .cso 로 저장할지 여부(기본: Debug 에서만)
#ifndef SHADER_CACHE_ENABLED
    #ifdef _DEBUG
        #define SHADER_CACHE_ENABLED 1
    #else
        #define SHADER_CACHE_ENABLED 0
    #endif
#endif

// -------------------------------------------------------------
// HRESULT 검사 헬퍼 : 실패 경로를 반드시 남긴다(체크리스트 항목)
// -------------------------------------------------------------
namespace dxutil
{
    void DebugLog(const wchar_t* format, ...);
    bool CheckHR(HRESULT hr, const wchar_t* what, const wchar_t* file, int line);
}

#define DX_CHECK(hr, what)  dxutil::CheckHR((hr), (what), __FILEW__, __LINE__)
#define DX_FAILED(hr, what) (!DX_CHECK((hr), (what)))
