#include "Core/stdafx.h"
#include "Editor/HierarchyPanel.h"
#include "Editor/EditorStyle.h"
#include "Engine/Scene.h"
#include "Engine/GameObject.h"
#include "Input/InputManager.h"
#include "Utils/StringUtil.h"

#include <cstdio>

// -------------------------------------------------------------
// 접기 / 펼치기 상태
// -------------------------------------------------------------
bool HierarchyPanel::IsCollapsed(uint64_t id) const
{
    return std::find(m_collapsed.begin(), m_collapsed.end(), id) != m_collapsed.end();
}

void HierarchyPanel::ToggleCollapsed(uint64_t id)
{
    auto it = std::find(m_collapsed.begin(), m_collapsed.end(), id);
    if (it != m_collapsed.end())
        m_collapsed.erase(it);
    else
        m_collapsed.push_back(id);
}

void HierarchyPanel::CancelDrag()
{
    m_dragging = false;
    m_dragId = 0;
    m_dropTargetId = 0;
    m_dropToRoot = false;
}

// -------------------------------------------------------------
// 트리를 화면에 보일 줄 목록으로 펼친다.
//  Scene 의 소유 순서가 아니라 실제 부모-자식 관계를 따라가므로
//  계층이 그대로 드러난다.
// -------------------------------------------------------------
void HierarchyPanel::AppendObject(GameObject* object, int depth)
{
    if (!object || object->IsPendingDestroy())
        return;

    std::vector<GameObject*> children;
    for (GameObject* child : object->GetChildren())
    {
        if (child && !child->IsPendingDestroy())
            children.push_back(child);
    }

    Row row;
    row.id = object->GetId();
    row.depth = depth;
    row.hasChildren = !children.empty();
    row.collapsed = IsCollapsed(row.id);
    row.active = object->IsActive();

    wchar_t buffer[192];
    _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%s   #%llu",
                 StringUtil::Utf8ToWide(object->GetName()).c_str(),
                 static_cast<unsigned long long>(row.id));
    row.label = buffer;

    m_rows.push_back(std::move(row));

    if (IsCollapsed(object->GetId()))
        return;

    for (GameObject* child : children)
        AppendObject(child, depth + 1);
}

void HierarchyPanel::BuildRows(Scene& scene)
{
    m_rows.clear();

    for (const auto& object : scene.GetGameObjects())
    {
        if (!object || object->IsPendingDestroy())
            continue;

        if (object->GetParent() == nullptr)   // 루트만 진입점. 자식은 재귀로 따라간다.
            AppendObject(object.get(), 0);
    }

    // 줄 사각형을 미리 계산해 둔다(같은 프레임에 입력 판정과 그리기가 일치하도록).
    int y = kPanelY + kHeaderHeight + kPadding;
    for (Row& row : m_rows)
    {
        row.rect = { kPanelX + 2, y, kPanelX + kPanelWidth - 2, y + kRowHeight };

        const int indent = kPanelX + kPadding + row.depth * kIndentWidth;
        row.arrow = { indent, y, indent + 14, y + kRowHeight };

        y += kRowHeight;
    }

    m_listBottom = y;
    m_panelHeight = (m_listBottom - kPanelY) + kDropZoneHeight + kPadding;
}

const HierarchyPanel::Row* HierarchyPanel::FindRowAt(int x, int y) const
{
    for (const Row& row : m_rows)
    {
        if (x >= row.rect.left && x < row.rect.right &&
            y >= row.rect.top && y < row.rect.bottom)
        {
            return &row;
        }
    }
    return nullptr;
}

// -------------------------------------------------------------
// 입력 처리
// -------------------------------------------------------------
HierarchyPanel::Action HierarchyPanel::Update(Scene& scene, const InputManager& input, uint64_t selectedId)
{
    Action action;

    if (!m_visible)
    {
        m_rows.clear();
        CancelDrag();
        return action;
    }

    BuildRows(scene);
    (void)selectedId;

    const int mouseX = input.GetMouseX();
    const int mouseY = input.GetMouseY();
    const bool inside = Contains(mouseX, mouseY);

    // ---- 누른 순간 ----
    if (input.GetMouseButtonDown(InputManager::Left) && inside)
    {
        if (const Row* row = FindRowAt(mouseX, mouseY))
        {
            // 화살표는 접기/펼치기 전용
            if (row->hasChildren && mouseX >= row->arrow.left && mouseX < row->arrow.right)
            {
                ToggleCollapsed(row->id);
            }
            else
            {
                action.selectId = row->id;

                // 아직 드래그는 아니다. 일정 거리 이상 움직여야 시작한다.
                m_dragId = row->id;
                m_pressX = mouseX;
                m_pressY = mouseY;
                m_dragging = false;
            }
        }
    }

    // ---- 누른 채 이동 : 임계값을 넘으면 드래그 시작 ----
    if (m_dragId != 0 && input.GetMouseButton(InputManager::Left))
    {
        if (!m_dragging)
        {
            const int dx = mouseX - m_pressX;
            const int dy = mouseY - m_pressY;
            if (dx * dx + dy * dy >= kDragThreshold * kDragThreshold)
                m_dragging = true;
        }

        if (m_dragging)
        {
            m_dropTargetId = 0;
            m_dropToRoot = false;

            if (inside)
            {
                if (const Row* row = FindRowAt(mouseX, mouseY))
                {
                    if (row->id != m_dragId)
                        m_dropTargetId = row->id;
                }
                else if (mouseY >= m_listBottom)
                {
                    m_dropToRoot = true;    // 목록 아래 빈 곳 = 루트로 분리
                }
            }
        }
    }

    // ---- 떼는 순간 : 재부모화 요청 ----
    if (m_dragId != 0 && !input.GetMouseButton(InputManager::Left))
    {
        if (m_dragging && (m_dropTargetId != 0 || m_dropToRoot))
        {
            action.reparent = true;
            action.dragId = m_dragId;
            action.newParentId = m_dropToRoot ? 0 : m_dropTargetId;
        }

        CancelDrag();
    }

    return action;
}

// -------------------------------------------------------------
// 그리기
// -------------------------------------------------------------
void HierarchyPanel::Draw(HDC hdc, uint64_t selectedId)
{
    if (!m_visible || !hdc)
        return;

    const int left = kPanelX;
    const int top = kPanelY;
    const int right = kPanelX + kPanelWidth;
    const int bottom = kPanelY + m_panelHeight;

    RECT panelRect = { left, top, right, bottom };
    editor::FillSolid(hdc, panelRect, editor::kPanelBackground);

    RECT headerRect = { left, top, right, top + kHeaderHeight };
    editor::FillSolid(hdc, headerRect, editor::kHeaderBackground);
    editor::FrameSolid(hdc, panelRect, editor::kPanelBorder);

    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, editor::GetUIFontBold()));
    const int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);

    editor::DrawLabel(hdc, left + kPadding + 2, top + 5, L"Hierarchy", editor::kTextNormal);

    ::SelectObject(hdc, editor::GetUIFont());
    editor::DrawLabel(hdc, right - 96, top + 6, L"H: 켜기/끄기", editor::kTextDim);

    // ---- 줄 ----
    for (const Row& row : m_rows)
    {
        const bool selected = (row.id == selectedId) && (selectedId != 0);
        const bool isDropTarget = m_dragging && (row.id == m_dropTargetId);
        const bool isBeingDragged = m_dragging && (row.id == m_dragId);

        if (isDropTarget)
            editor::FillSolid(hdc, row.rect, editor::kDropTargetRow);
        else if (selected)
            editor::FillSolid(hdc, row.rect, editor::kSelectedRow);

        const int indent = row.arrow.left;

        if (row.hasChildren)
        {
            editor::DrawLabel(hdc, indent, row.rect.top + 2,
                              row.collapsed ? L"▶" : L"▼",
                              selected ? editor::kTextSelected : editor::kTextDim);
        }

        COLORREF textColor = row.active ? editor::kTextNormal : editor::kTextInactive;
        if (selected || isDropTarget) textColor = editor::kTextSelected;
        if (isBeingDragged)           textColor = editor::kTextDim;

        editor::DrawLabel(hdc, indent + 16, row.rect.top + 2, row.label, textColor);
    }

    // ---- 목록 아래 : 루트로 분리 영역 ----
    RECT dropZone = { left + 2, m_listBottom, right - 2, m_listBottom + kDropZoneHeight };
    if (m_dragging)
    {
        if (m_dropToRoot)
            editor::FillSolid(hdc, dropZone, editor::kDropTargetRow);

        editor::DrawLabel(hdc, left + kPadding, m_listBottom + 3,
                          L"여기에 놓으면 루트로 분리",
                          m_dropToRoot ? editor::kTextSelected : editor::kTextDim);
    }
    else
    {
        editor::DrawLabel(hdc, left + kPadding, m_listBottom + 3,
                          L"끌어서 부모 바꾸기", editor::kTextDim);
    }

    ::SetBkMode(hdc, oldBkMode);
    ::SelectObject(hdc, oldFont);
}

bool HierarchyPanel::Contains(int x, int y) const
{
    if (!m_visible)
        return false;

    return x >= kPanelX && x < kPanelX + kPanelWidth &&
           y >= kPanelY && y < kPanelY + m_panelHeight;
}
