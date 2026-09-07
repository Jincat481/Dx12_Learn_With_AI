#include "Core/stdafx.h"
#include "Core/Window.h"
#include "Input/InputManager.h"

// windowsx.h 의 GET_X_LPARAM 과 같은 역할. 음수 좌표(창 밖)를 위해 short 로 캐스팅한다.
#define GET_X_LPARAM_COMPAT(lp) (static_cast<int>(static_cast<short>(LOWORD(lp))))
#define GET_Y_LPARAM_COMPAT(lp) (static_cast<int>(static_cast<short>(HIWORD(lp))))

Window::~Window()
{
    Destroy();
}

bool Window::Create(HINSTANCE hInstance, const std::wstring& title, int width, int height)
{
    m_hInstance = hInstance;
    m_width = width;
    m_height = height;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &Window::WndProcSetup;
    wc.hInstance     = hInstance;
    wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;                 // D3D 가 직접 그리므로 배경을 지우지 않는다.
    wc.lpszClassName = kClassName;

    if (!::RegisterClassExW(&wc))
    {
        // 이미 등록되어 있는 경우는 통과시킨다.
        if (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            dxutil::DebugLog(L"[Window] RegisterClassExW 실패");
            return false;
        }
    }

    // 클라이언트 영역이 정확히 width x height 가 되도록 창 크기를 보정한다.
    const DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    RECT rect = { 0, 0, width, height };
    ::AdjustWindowRect(&rect, style, FALSE);

    m_hwnd = ::CreateWindowExW(
        0, kClassName, title.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, this);      // lpParam 으로 this 를 넘겨 WndProc 와 연결한다.

    if (!m_hwnd)
    {
        dxutil::DebugLog(L"[Window] CreateWindowExW 실패");
        return false;
    }

    ::ShowWindow(m_hwnd, SW_SHOW);
    ::UpdateWindow(m_hwnd);
    return true;
}

void Window::Destroy()
{
    if (m_hwnd)
    {
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void Window::SetTitle(const std::wstring& title)
{
    if (m_hwnd)
        ::SetWindowTextW(m_hwnd, title.c_str());
}

bool Window::ProcessMessages()
{
    MSG msg = {};
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return false;

        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    return true;
}

// -------------------------------------------------------------
// WM_NCCREATE 시점에 this 포인터를 창에 묶고, 이후에는 Thunk 로 넘긴다.
// -------------------------------------------------------------
LRESULT CALLBACK Window::WndProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        const CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        Window* self = static_cast<Window*>(create->lpCreateParams);

        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::WndProcThunk));

        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK Window::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* self = reinterpret_cast<Window*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self)
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);

    return self->HandleMessage(hwnd, msg, wParam, lParam);
}

LRESULT Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    InputManager& input = InputManager::Get();

    switch (msg)
    {
    case WM_CLOSE:
        ::PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        m_hwnd = nullptr;
        ::PostQuitMessage(0);
        return 0;

    // ---- 키보드 ----
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        // ESC 종료는 Game 에서 판단한다. (인스펙터 편집 중이면 편집 취소가 우선)
        input.OnKeyDown(wParam);
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        input.OnKeyUp(wParam);
        return 0;

    // 인스펙터 텍스트 입력. Enter(\r), Backspace(\b), Esc(0x1B) 도 여기로 들어온다.
    case WM_CHAR:
        input.OnChar(static_cast<wchar_t>(wParam));
        return 0;

    // ---- 마우스 ----
    case WM_LBUTTONDOWN: input.OnMouseButtonDown(InputManager::Left);   return 0;
    case WM_LBUTTONUP:   input.OnMouseButtonUp(InputManager::Left);     return 0;
    case WM_RBUTTONDOWN: input.OnMouseButtonDown(InputManager::Right);  return 0;
    case WM_RBUTTONUP:   input.OnMouseButtonUp(InputManager::Right);    return 0;
    case WM_MBUTTONDOWN: input.OnMouseButtonDown(InputManager::Middle); return 0;
    case WM_MBUTTONUP:   input.OnMouseButtonUp(InputManager::Middle);   return 0;

    case WM_MOUSEMOVE:
        input.OnMouseMove(GET_X_LPARAM_COMPAT(lParam), GET_Y_LPARAM_COMPAT(lParam));
        return 0;

    case WM_MOUSEWHEEL:
        input.OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    // 포커스를 잃으면 눌린 키가 남지 않도록 정리한다.
    case WM_KILLFOCUS:
        input.Clear();
        return 0;

    default:
        break;
    }

    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}
