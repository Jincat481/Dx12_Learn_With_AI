#pragma once
#include "Core/stdafx.h"

// =============================================================
// InputManager (과제 1 / S06)
//  키 상태 머신
//      None    --(WM_KEYDOWN)--> Down
//      Down    --(다음 Update)--> Pressed
//      Pressed --(WM_KEYUP)-----> Up
//      Up      --(다음 Update)--> None
//
//  Win32 메시지는 Window 에서 곧바로 큐(pending)에 쌓아두고,
//  게임 루프의 "입력 갱신" 단계에서 한 번에 상태로 반영한다.
//  이렇게 해야 Down / Up 이 정확히 한 프레임만 참(True)이 된다.
// =============================================================
class InputManager
{
public:
    enum class KeyState { None, Down, Pressed, Up };

    enum MouseButton { Left = 0, Right = 1, Middle = 2, MouseButtonCount };

    static InputManager& Get();

    void Update();          // 게임 루프의 "입력 갱신" 단계에서 호출
    void Clear();           // 창이 포커스를 잃었을 때 등

    // --- Win32 메시지 반영 (Window 에서 호출) ---
    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);
    void OnMouseButtonDown(MouseButton button);
    void OnMouseButtonUp(MouseButton button);
    void OnMouseMove(int x, int y);
    void OnMouseWheel(int delta);
    void OnChar(wchar_t character);           // WM_CHAR

    // ---- 텍스트 입력 ----
    // 이번 프레임에 입력된 문자들. Enter(\r), Backspace(\b), Esc(0x1B) 도 그대로 들어온다.
    const std::wstring& GetTypedText() const { return m_typedText; }

    // 에디터 UI 가 텍스트를 입력받는 동안에는 게임 쪽 키 조회를 막는다.
    // (이름을 치는데 'D' 때문에 캐릭터가 움직이면 안 된다)
    void SetTextCaptureActive(bool active) { m_textCapture = active; }
    bool IsTextCaptureActive() const { return m_textCapture; }

    // --- 조회 ---
    bool GetKey(int virtualKey) const;        // 누르고 있는 동안 계속 true
    bool GetKeyDown(int virtualKey) const;    // 누른 그 프레임만 true
    bool GetKeyUp(int virtualKey) const;      // 뗀 그 프레임만 true

    bool GetMouseButton(MouseButton button) const;
    bool GetMouseButtonDown(MouseButton button) const;
    bool GetMouseButtonUp(MouseButton button) const;

    int  GetMouseX() const { return m_mouseX; }
    int  GetMouseY() const { return m_mouseY; }
    int  GetMouseWheelDelta() const { return m_wheelDelta; }

private:
    InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    static void AdvanceState(KeyState& state);
    static void ApplyDown(KeyState& state);
    static void ApplyUp(KeyState& state);

    static constexpr int kKeyCount = 256;

    KeyState m_keys[kKeyCount] = {};
    bool     m_pendingKeyDown[kKeyCount] = {};
    bool     m_pendingKeyUp[kKeyCount] = {};

    KeyState m_mouse[MouseButtonCount] = {};
    bool     m_pendingMouseDown[MouseButtonCount] = {};
    bool     m_pendingMouseUp[MouseButtonCount] = {};

    int m_mouseX = 0;
    int m_mouseY = 0;
    int m_wheelDelta = 0;
    int m_pendingWheelDelta = 0;

    std::wstring m_pendingText;   // 메시지에서 쌓이는 중
    std::wstring m_typedText;     // 이번 프레임 확정본
    bool         m_textCapture = false;
};
