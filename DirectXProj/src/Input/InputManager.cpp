#include "Core/stdafx.h"
#include "Input/InputManager.h"

InputManager& InputManager::Get()
{
    static InputManager instance;
    return instance;
}

void InputManager::AdvanceState(KeyState& state)
{
    if (state == KeyState::Down) state = KeyState::Pressed;
    else if (state == KeyState::Up) state = KeyState::None;
}

void InputManager::ApplyDown(KeyState& state)
{
    // 키 자동 반복(auto repeat)으로 Down 이 다시 발생해도 Pressed 를 유지한다.
    if (state == KeyState::None || state == KeyState::Up)
        state = KeyState::Down;
}

void InputManager::ApplyUp(KeyState& state)
{
    if (state == KeyState::Down || state == KeyState::Pressed)
        state = KeyState::Up;
    else
        state = KeyState::None;
}

void InputManager::Update()
{
    // 1) 지난 프레임 상태를 한 칸 진행시킨다.
    for (int i = 0; i < kKeyCount; ++i)
        AdvanceState(m_keys[i]);
    for (int i = 0; i < MouseButtonCount; ++i)
        AdvanceState(m_mouse[i]);

    // 2) 이번 프레임 메시지에서 쌓인 이벤트를 반영한다.
    for (int i = 0; i < kKeyCount; ++i)
    {
        if (m_pendingKeyDown[i]) { ApplyDown(m_keys[i]); m_pendingKeyDown[i] = false; }
        if (m_pendingKeyUp[i])   { ApplyUp(m_keys[i]);   m_pendingKeyUp[i] = false; }
    }
    for (int i = 0; i < MouseButtonCount; ++i)
    {
        if (m_pendingMouseDown[i]) { ApplyDown(m_mouse[i]); m_pendingMouseDown[i] = false; }
        if (m_pendingMouseUp[i])   { ApplyUp(m_mouse[i]);   m_pendingMouseUp[i] = false; }
    }

    m_wheelDelta = m_pendingWheelDelta;
    m_pendingWheelDelta = 0;

    m_typedText = std::move(m_pendingText);
    m_pendingText.clear();
}

void InputManager::Clear()
{
    for (int i = 0; i < kKeyCount; ++i)
    {
        m_keys[i] = KeyState::None;
        m_pendingKeyDown[i] = false;
        m_pendingKeyUp[i] = false;
    }
    for (int i = 0; i < MouseButtonCount; ++i)
    {
        m_mouse[i] = KeyState::None;
        m_pendingMouseDown[i] = false;
        m_pendingMouseUp[i] = false;
    }
    m_wheelDelta = 0;
    m_pendingWheelDelta = 0;
    m_pendingText.clear();
    m_typedText.clear();
}

void InputManager::OnKeyDown(WPARAM key)
{
    if (key < kKeyCount)
        m_pendingKeyDown[key] = true;
}

void InputManager::OnKeyUp(WPARAM key)
{
    if (key < kKeyCount)
        m_pendingKeyUp[key] = true;
}

void InputManager::OnMouseButtonDown(MouseButton button)
{
    if (button >= 0 && button < MouseButtonCount)
        m_pendingMouseDown[button] = true;
}

void InputManager::OnMouseButtonUp(MouseButton button)
{
    if (button >= 0 && button < MouseButtonCount)
        m_pendingMouseUp[button] = true;
}

void InputManager::OnMouseMove(int x, int y)
{
    m_mouseX = x;
    m_mouseY = y;
}

void InputManager::OnMouseWheel(int delta)
{
    m_pendingWheelDelta += delta;
}

void InputManager::OnChar(wchar_t character)
{
    if (m_pendingText.size() < 256)
        m_pendingText.push_back(character);
}

bool InputManager::GetKey(int virtualKey) const
{
    if (m_textCapture) return false;   // 에디터가 텍스트를 받는 중
    if (virtualKey < 0 || virtualKey >= kKeyCount) return false;
    return m_keys[virtualKey] == KeyState::Down || m_keys[virtualKey] == KeyState::Pressed;
}

bool InputManager::GetKeyDown(int virtualKey) const
{
    if (m_textCapture) return false;   // 에디터가 텍스트를 받는 중
    if (virtualKey < 0 || virtualKey >= kKeyCount) return false;
    return m_keys[virtualKey] == KeyState::Down;
}

bool InputManager::GetKeyUp(int virtualKey) const
{
    if (m_textCapture) return false;   // 에디터가 텍스트를 받는 중
    if (virtualKey < 0 || virtualKey >= kKeyCount) return false;
    return m_keys[virtualKey] == KeyState::Up;
}

bool InputManager::GetMouseButton(MouseButton button) const
{
    if (button < 0 || button >= MouseButtonCount) return false;
    return m_mouse[button] == KeyState::Down || m_mouse[button] == KeyState::Pressed;
}

bool InputManager::GetMouseButtonDown(MouseButton button) const
{
    if (button < 0 || button >= MouseButtonCount) return false;
    return m_mouse[button] == KeyState::Down;
}

bool InputManager::GetMouseButtonUp(MouseButton button) const
{
    if (button < 0 || button >= MouseButtonCount) return false;
    return m_mouse[button] == KeyState::Up;
}
