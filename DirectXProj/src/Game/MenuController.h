#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Editor/MenuScreen.h"

class Graphics;

// =============================================================
// MenuController
//  메인 메뉴를 "특별한 상태" 가 아니라 **평범한 씬 안의 컴포넌트** 로 만든다.
//
//  Scene 을 상속하지 않는다. 이 엔진의 확장 방식은 상속이 아니라
//  GameObject 에 Component 를 붙이는 합성이기 때문이다.
//      MainMenu 씬 → MenuRoot 오브젝트 → MenuController 컴포넌트
//
//  덕분에 메뉴도 SceneManager 가 다른 씬과 똑같이 Update / Render 한다.
//  메뉴 배경에 지형이나 스프라이트를 띄우고 싶으면 GameObject 를 더하면 된다.
//
//  주의 : 항목을 고른 그 자리에서 씬을 바꾸면 안 된다.
//         지금 이 컴포넌트를 돌리고 있는 씬이 통째로 파괴되기 때문이다.
//         그래서 선택은 콜백으로 "요청" 만 남기고, Game 이 프레임 끝에 처리한다.
// =============================================================
class MenuController : public Component
{
public:
    using SelectCallback = std::function<void(int)>;

    MenuController() = default;
    ~MenuController() override = default;

    const char* GetTypeName() const override { return "MenuController"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;

    // GDI 오버레이 DC 는 프레임당 하나뿐이라 Game 의 단일 오버레이 패스에서 불린다.
    void DrawOverlay(HDC hdc);

    void SetEntries(std::vector<MenuScreen::Entry> entries);
    void SetOnSelect(SelectCallback callback) { m_onSelect = std::move(callback); }

private:
    Graphics*      m_graphics = nullptr;
    MenuScreen     m_screen;
    SelectCallback m_onSelect;
};
