#pragma once
#include "Core/stdafx.h"

class Graphics;
class GameObject;
class InputManager;

// =============================================================
// InspectorPanel
//  Unity 의 Inspector 창처럼 선택된 GameObject 의 값을 보여주고 편집한다.
//
//  - 이름 (문자 입력)
//  - Transform : 로컬 위치 / 회전(오일러 각, 도) / 스케일
//  - 붙어 있는 Component 목록
//
//  칸을 클릭하면 편집 모드가 되고, Enter 로 확정 / Esc 로 취소한다.
//  편집 중에는 InputManager 의 게임 키 조회가 막히므로
//  이름에 'D' 를 쳐도 캐릭터가 움직이지 않는다.
// =============================================================
class InspectorPanel
{
public:
    InspectorPanel() = default;

    // 레이아웃 계산 + 입력 처리. target 이 없으면 안내만 표시한다.
    void Update(const InputManager& input, GameObject* target, int viewportWidth);

    void Draw(HDC hdc);

    bool Contains(int x, int y) const;
    bool IsEditing() const { return m_editing != FieldId::None; }
    void CancelEdit();

    bool IsVisible() const { return m_visible; }
    void Toggle() { m_visible = !m_visible; CancelEdit(); }

private:
    enum class FieldId
    {
        None,
        Name,
        PosX, PosY, PosZ,
        RotX, RotY, RotZ,
        ScaleX, ScaleY, ScaleZ,
    };

    struct Field
    {
        FieldId      id = FieldId::None;
        RECT         rect{};
        std::wstring text;      // 표시용 현재 값
    };

    struct Line       // 편집 칸이 아닌 표시 전용 줄
    {
        std::wstring text;
        int  x = 0;
        int  y = 0;
        bool dim = false;
        bool bold = false;
    };

    void BuildLayout(GameObject* target, int viewportWidth);
    void AddField(FieldId id, int x, int y, int width, const std::wstring& text);

    void BeginEdit(FieldId id);
    void CommitEdit(GameObject* target);
    void ApplyTypedText(const std::wstring& typed, GameObject* target);

    const Field* FindFieldAt(int x, int y) const;

    std::vector<Field> m_fields;
    std::vector<Line>  m_lines;

    bool         m_visible = true;
    FieldId      m_editing = FieldId::None;
    std::wstring m_editBuffer;

    // 레이아웃
    static constexpr int kPanelWidth = 300;
    static constexpr int kMarginRight = 12;
    static constexpr int kPanelY = 12;
    static constexpr int kHeaderHeight = 26;
    static constexpr int kRowHeight = 22;
    static constexpr int kFieldHeight = 18;
    static constexpr int kPadding = 8;
    static constexpr int kLabelWidth = 62;

    int m_panelX = 0;
    int m_panelHeight = kHeaderHeight + kPadding * 2;
};
