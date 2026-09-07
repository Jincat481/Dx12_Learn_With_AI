#pragma once
#include "Core/stdafx.h"

// =============================================================
// Window (과제 1 / S02)
//  Win32 창 생성과 메시지 루프를 담당하고,
//  키보드/마우스 메시지를 InputManager 로 전달한다.
// =============================================================
class Window
{
public:
    Window() = default;
    ~Window();

    bool Create(HINSTANCE hInstance, const std::wstring& title, int width, int height);
    void Destroy();

    // 큐에 쌓인 메시지를 모두 처리한다. WM_QUIT 를 받으면 false 를 돌려준다.
    bool ProcessMessages();

    HWND GetHandle() const { return m_hwnd; }
    int  GetWidth()  const { return m_width; }
    int  GetHeight() const { return m_height; }

    void SetTitle(const std::wstring& title);

private:
    static LRESULT CALLBACK WndProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE m_hInstance = nullptr;
    HWND      m_hwnd = nullptr;
    int       m_width = 0;
    int       m_height = 0;

    static constexpr const wchar_t* kClassName = L"DirectXProjWindowClass";
};
