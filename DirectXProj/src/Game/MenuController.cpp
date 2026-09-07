#include "Core/stdafx.h"
#include "Game/MenuController.h"
#include "Core/Graphics.h"
#include "Input/InputManager.h"

void MenuController::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
}

void MenuController::SetEntries(std::vector<MenuScreen::Entry> entries)
{
    m_screen.SetEntries(std::move(entries));
}

void MenuController::Update()
{
    if (!m_graphics)
        return;

    const int choice = m_screen.Update(InputManager::Get(),
                                       m_graphics->GetWidth(),
                                       m_graphics->GetHeight());

    // 여기서 씬을 바꾸면 지금 순회 중인 씬이 파괴된다.
    // 요청만 넘기고 실제 전환은 Game 이 프레임 끝에 한다.
    if (choice != MenuScreen::kNoSelection && m_onSelect)
        m_onSelect(choice);
}

void MenuController::DrawOverlay(HDC hdc)
{
    if (!hdc || !m_graphics)
        return;

    m_screen.Draw(hdc, m_graphics->GetWidth(), m_graphics->GetHeight());
}
