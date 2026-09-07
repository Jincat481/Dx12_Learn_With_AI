#include "Core/stdafx.h"
#include "Editor/InspectorPanel.h"
#include "Editor/EditorStyle.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Input/InputManager.h"
#include "Utils/StringUtil.h"

using namespace DirectX;

namespace
{
    constexpr wchar_t kCharBackspace = 0x08;
    constexpr wchar_t kCharReturn    = 0x0D;
    constexpr wchar_t kCharEscape    = 0x1B;
    constexpr wchar_t kCharTab       = 0x09;
}

// -------------------------------------------------------------
// 레이아웃
// -------------------------------------------------------------
void InspectorPanel::AddField(FieldId id, int x, int y, int width, const std::wstring& text)
{
    Field field;
    field.id = id;
    field.rect = { x, y, x + width, y + kFieldHeight };
    field.text = text;
    m_fields.push_back(std::move(field));
}

void InspectorPanel::BuildLayout(GameObject* target, int viewportWidth)
{
    m_fields.clear();
    m_lines.clear();

    m_panelX = viewportWidth - kPanelWidth - kMarginRight;

    const int left = m_panelX;
    int y = kPanelY + kHeaderHeight + kPadding;

    if (!target)
    {
        m_lines.push_back({ L"선택된 오브젝트가 없습니다.", left + kPadding, y, true, false });
        y += kRowHeight;
        m_lines.push_back({ L"스프라이트를 클릭하거나", left + kPadding, y, true, false });
        y += kRowHeight - 4;
        m_lines.push_back({ L"Hierarchy 에서 골라야합니다.", left + kPadding, y, true, false });
        y += kRowHeight;

        m_panelHeight = (y - kPanelY) + kPadding;
        return;
    }

    const int fieldLeft = left + kPadding + kLabelWidth;
    const int wideFieldWidth = kPanelWidth - kPadding * 2 - kLabelWidth;

    // ---- 이름 ----
    m_lines.push_back({ L"Name", left + kPadding, y + 2, false, false });
    AddField(FieldId::Name, fieldLeft, y, wideFieldWidth,
             StringUtil::Utf8ToWide(target->GetName()));
    y += kRowHeight;

    // ---- 구분선 자리 + Transform ----
    y += 4;
    m_lines.push_back({ L"Transform", left + kPadding, y, false, true });
    y += kRowHeight;

    Transform* transform = target->GetTransform();
    if (transform)
    {
        const int boxWidth = (wideFieldWidth - 8) / 3;

        const XMFLOAT3& position = transform->GetLocalPosition();
        const XMFLOAT3  euler    = transform->GetLocalRotationEuler();
        const XMFLOAT3& scale    = transform->GetLocalScale();

        struct RowSpec
        {
            const wchar_t* label;
            FieldId x, y, z;
            XMFLOAT3 value;
        };

        const RowSpec rows[3] =
        {
            { L"Position", FieldId::PosX,   FieldId::PosY,   FieldId::PosZ,   position },
            { L"Rotation", FieldId::RotX,   FieldId::RotY,   FieldId::RotZ,   euler    },
            { L"Scale",    FieldId::ScaleX, FieldId::ScaleY, FieldId::ScaleZ, scale    },
        };

        for (const RowSpec& row : rows)
        {
            m_lines.push_back({ row.label, left + kPadding, y + 2, false, false });

            AddField(row.x, fieldLeft,                     y, boxWidth, editor::FormatFloat(row.value.x));
            AddField(row.y, fieldLeft + boxWidth + 4,      y, boxWidth, editor::FormatFloat(row.value.y));
            AddField(row.z, fieldLeft + (boxWidth + 4) * 2, y, boxWidth, editor::FormatFloat(row.value.z));

            y += kRowHeight;
        }

        // 회전은 오일러 각(도) 표기임을 알려준다.
        m_lines.push_back({ L"* Rotation 은 오일러 각(도)", left + kPadding, y, true, false });
        y += kRowHeight - 2;
    }

    // ---- Component 목록 ----
    y += 4;
    m_lines.push_back({ L"Components", left + kPadding, y, false, true });
    y += kRowHeight;

    for (const auto& bucket : target->GetComponentMap())
    {
        for (const auto& component : bucket.second)
        {
            if (!component || component->IsPendingDestroy())
                continue;

            std::wstring text = L"• ";
            text += StringUtil::Utf8ToWide(component->GetTypeName());
            if (!component->IsEnabled())
                text += L"  (disabled)";

            m_lines.push_back({ text, left + kPadding + 4, y, false, false });
            y += kRowHeight - 4;
        }
    }

    y += kPadding;
    m_panelHeight = (y - kPanelY);
}

// -------------------------------------------------------------
// 편집
// -------------------------------------------------------------
void InspectorPanel::BeginEdit(FieldId id)
{
    m_editing = id;
    m_editBuffer.clear();

    for (const Field& field : m_fields)
    {
        if (field.id == id)
        {
            m_editBuffer = field.text;    // 현재 값에서 시작한다
            break;
        }
    }
}

void InspectorPanel::CancelEdit()
{
    m_editing = FieldId::None;
    m_editBuffer.clear();
}

void InspectorPanel::CommitEdit(GameObject* target)
{
    if (m_editing == FieldId::None || !target)
    {
        CancelEdit();
        return;
    }

    const FieldId id = m_editing;
    const std::wstring text = m_editBuffer;

    if (id == FieldId::Name)
    {
        // 빈 이름은 받지 않는다.
        std::wstring trimmed = text;
        while (!trimmed.empty() && trimmed.front() == L' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == L' ')  trimmed.pop_back();

        if (!trimmed.empty())
            target->SetName(StringUtil::WideToUtf8(trimmed));

        CancelEdit();
        return;
    }

    Transform* transform = target->GetTransform();
    if (!transform)
    {
        CancelEdit();
        return;
    }

    float value = 0.0f;
    if (!editor::ParseFloat(text, value))
    {
        dxutil::DebugLog(L"[Inspector] 숫자로 읽을 수 없어 되돌린다 : %s", text.c_str());
        CancelEdit();
        return;
    }

    XMFLOAT3 position = transform->GetLocalPosition();
    XMFLOAT3 euler    = transform->GetLocalRotationEuler();
    XMFLOAT3 scale    = transform->GetLocalScale();

    switch (id)
    {
    case FieldId::PosX: position.x = value; transform->SetLocalPosition(position); break;
    case FieldId::PosY: position.y = value; transform->SetLocalPosition(position); break;
    case FieldId::PosZ: position.z = value; transform->SetLocalPosition(position); break;

    case FieldId::RotX: euler.x = value; transform->SetLocalRotationEuler(euler.x, euler.y, euler.z); break;
    case FieldId::RotY: euler.y = value; transform->SetLocalRotationEuler(euler.x, euler.y, euler.z); break;
    case FieldId::RotZ: euler.z = value; transform->SetLocalRotationEuler(euler.x, euler.y, euler.z); break;

    // 스케일 0 은 월드 행렬의 역행렬을 못 구하게 만든다(피킹·드래그가 죽는다).
    case FieldId::ScaleX: scale.x = (value == 0.0f) ? 0.0001f : value; transform->SetLocalScale(scale); break;
    case FieldId::ScaleY: scale.y = (value == 0.0f) ? 0.0001f : value; transform->SetLocalScale(scale); break;
    case FieldId::ScaleZ: scale.z = (value == 0.0f) ? 0.0001f : value; transform->SetLocalScale(scale); break;

    default: break;
    }

    CancelEdit();
}

void InspectorPanel::ApplyTypedText(const std::wstring& typed, GameObject* target)
{
    for (wchar_t c : typed)
    {
        if (c == kCharReturn)
        {
            CommitEdit(target);
            return;
        }

        if (c == kCharEscape)
        {
            CancelEdit();
            return;
        }

        if (c == kCharBackspace)
        {
            if (!m_editBuffer.empty())
                m_editBuffer.pop_back();
            continue;
        }

        if (c == kCharTab)
            continue;

        if (c < 0x20)          // 그 밖의 제어 문자는 무시
            continue;

        if (m_editBuffer.size() < 64)
            m_editBuffer.push_back(c);
    }
}

const InspectorPanel::Field* InspectorPanel::FindFieldAt(int x, int y) const
{
    for (const Field& field : m_fields)
    {
        if (x >= field.rect.left && x < field.rect.right &&
            y >= field.rect.top && y < field.rect.bottom)
        {
            return &field;
        }
    }
    return nullptr;
}

// -------------------------------------------------------------
// 갱신
// -------------------------------------------------------------
void InspectorPanel::Update(const InputManager& input, GameObject* target, int viewportWidth)
{
    if (!m_visible)
    {
        m_fields.clear();
        m_lines.clear();
        CancelEdit();
        return;
    }

    // 대상이 사라졌으면 편집을 접는다.
    if (!target && m_editing != FieldId::None)
        CancelEdit();

    BuildLayout(target, viewportWidth);

    // 편집 중인 칸은 표시 값 대신 편집 버퍼를 보여준다.
    if (m_editing != FieldId::None)
    {
        for (Field& field : m_fields)
        {
            if (field.id == m_editing)
            {
                field.text = m_editBuffer;
                break;
            }
        }
    }

    // ---- 클릭 ----
    if (input.GetMouseButtonDown(InputManager::Left))
    {
        const int mouseX = input.GetMouseX();
        const int mouseY = input.GetMouseY();

        if (const Field* field = FindFieldAt(mouseX, mouseY))
        {
            if (m_editing != field->id)
            {
                CommitEdit(target);        // 다른 칸을 편집 중이었다면 먼저 확정
                BeginEdit(field->id);
            }
        }
        else if (Contains(mouseX, mouseY))
        {
            CommitEdit(target);            // 패널 빈 곳 클릭 = 확정
        }
        else
        {
            CommitEdit(target);            // 바깥 클릭 = 확정
        }
    }

    // ---- 문자 입력 ----
    if (m_editing != FieldId::None)
    {
        const std::wstring& typed = input.GetTypedText();
        if (!typed.empty())
            ApplyTypedText(typed, target);

        // 버퍼가 바뀌었을 수 있으니 표시도 맞춰 준다.
        for (Field& field : m_fields)
        {
            if (field.id == m_editing)
            {
                field.text = m_editBuffer;
                break;
            }
        }
    }
}

// -------------------------------------------------------------
// 그리기
// -------------------------------------------------------------
void InspectorPanel::Draw(HDC hdc)
{
    if (!m_visible || !hdc)
        return;

    const int left = m_panelX;
    const int top = kPanelY;
    const int right = left + kPanelWidth;
    const int bottom = top + m_panelHeight;

    RECT panelRect = { left, top, right, bottom };
    editor::FillSolid(hdc, panelRect, editor::kPanelBackground);

    RECT headerRect = { left, top, right, top + kHeaderHeight };
    editor::FillSolid(hdc, headerRect, editor::kHeaderBackground);
    editor::FrameSolid(hdc, panelRect, editor::kPanelBorder);

    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, editor::GetUIFontBold()));
    const int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);

    editor::DrawLabel(hdc, left + kPadding, top + 5, L"Inspector", editor::kTextNormal);
    ::SelectObject(hdc, editor::GetUIFont());
    editor::DrawLabel(hdc, right - 96, top + 6, L"I: 켜기/끄기", editor::kTextDim);

    // ---- 표시 전용 줄 ----
    for (const Line& line : m_lines)
    {
        ::SelectObject(hdc, line.bold ? editor::GetUIFontBold() : editor::GetUIFont());
        editor::DrawLabel(hdc, line.x, line.y, line.text,
                          line.dim ? editor::kTextDim : editor::kTextNormal);
    }
    ::SelectObject(hdc, editor::GetUIFont());

    // ---- 편집 칸 ----
    for (const Field& field : m_fields)
    {
        const bool editing = (field.id == m_editing);

        editor::FillSolid(hdc, field.rect, editing ? editor::kFieldEditing : editor::kFieldBackground);
        editor::FrameSolid(hdc, field.rect, editor::kFieldBorder);

        std::wstring text = field.text;
        if (editing)
            text += L"|";                  // 커서

        // 칸을 넘치면 뒤쪽(입력 중인 부분)이 보이도록 앞을 자른다.
        const int maxChars = (field.rect.right - field.rect.left - 8) / 7;
        if (maxChars > 1 && static_cast<int>(text.size()) > maxChars)
            text = text.substr(text.size() - static_cast<size_t>(maxChars));

        editor::DrawLabel(hdc, field.rect.left + 4, field.rect.top + 1, text,
                          editing ? editor::kTextSelected : editor::kTextNormal);
    }

    ::SetBkMode(hdc, oldBkMode);
    ::SelectObject(hdc, oldFont);
}

bool InspectorPanel::Contains(int x, int y) const
{
    if (!m_visible)
        return false;

    return x >= m_panelX && x < m_panelX + kPanelWidth &&
           y >= kPanelY && y < kPanelY + m_panelHeight;
}
