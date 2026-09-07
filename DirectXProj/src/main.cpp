#include "Core/stdafx.h"
#include "Core/Game.h"

#include <cstdio>

// =============================================================
// 진입점
//  Win32 GUI 앱이므로 wWinMain 을 사용한다.
//  Debug 빌드에서는 로그 확인용 콘솔을 하나 붙인다.
// =============================================================
int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine,
                    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

#ifdef _DEBUG
    if (::AllocConsole())
    {
        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        ::SetConsoleTitleW(L"DirectXProj - Log");
        ::SetConsoleOutputCP(CP_UTF8);

        // QuickEdit 모드를 끈다.
        //  켜져 있으면 콘솔 창을 클릭하는 순간 "선택 모드"로 들어가고,
        //  그 상태에서 앱이 로그를 출력하면 WriteConsole 이 무한 대기해
        //  게임 전체가 멈춘 것처럼 보인다. (Enter/Esc 를 누르기 전까지)
        if (HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE); input != INVALID_HANDLE_VALUE)
        {
            DWORD mode = 0;
            if (::GetConsoleMode(input, &mode))
            {
                mode &= ~ENABLE_QUICK_EDIT_MODE;
                mode |= ENABLE_EXTENDED_FLAGS;
                ::SetConsoleMode(input, mode);
            }
        }
    }
#endif

    int exitCode = 0;
    {
        Game game;
        if (!game.Initialize(hInstance, 1280, 720))
        {
            dxutil::DebugLog(L"[main] 초기화 실패");
            exitCode = -1;
        }
        else
        {
            exitCode = game.Run();
        }
    }   // Game 소멸자에서 Shutdown

#ifdef _DEBUG
    ::FreeConsole();
#endif

    return exitCode;
}
