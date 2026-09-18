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

    // 창 크기가 바뀌면 부른다 (클라이언트 영역의 새 크기). Graphics 가 백버퍼를 다시 만든다.
    void SetResizeHandler(std::function<void(int, int)> handler) { m_onResize = std::move(handler); }

    // 테두리 없는 전체 화면 (F11). 독점 모드가 아니라 모니터를 덮는 창이라 Alt+Tab 이 자연스럽다.
    void ToggleFullscreen();
    bool IsFullscreen() const { return m_fullscreen; }

private:
    static LRESULT CALLBACK WndProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE m_hInstance = nullptr;
    HWND      m_hwnd = nullptr;
    int       m_width = 0;
    int       m_height = 0;

    std::function<void(int, int)> m_onResize;

    bool            m_fullscreen = false;
    DWORD           m_savedStyle = 0;
    WINDOWPLACEMENT m_savedPlacement{ sizeof(WINDOWPLACEMENT) };   // 전체 화면 전의 위치 · 크기

    static constexpr const wchar_t* kClassName = L"DirectXProjWindowClass";

    // 너무 작게 줄이면 조작 패널이 화면을 덮고 종횡비가 극단적으로 찌그러진다
    static constexpr int kMinClientWidth = 960;
    static constexpr int kMinClientHeight = 600;
};
