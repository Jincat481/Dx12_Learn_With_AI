#include "Core/stdafx.h"
#include "Terrain/TerrainBrush.h"
#include "Terrain/ChunkedTerrainRenderer.h"
#include "Terrain/HeightGrid.h"
#include "Engine/GameObject.h"
#include "Engine/GroundRaycast.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Input/InputManager.h"

#include <cmath>

using namespace DirectX;

void TerrainBrush::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
}

const wchar_t* TerrainBrush::GetToolName() const
{
    switch (m_tool)
    {
    case Tool::Raise:   return L"올리기";
    case Tool::Lower:   return L"내리기";
    case Tool::Flatten: return L"평탄화";
    case Tool::Smooth:  return L"부드럽게";
    default:            return L"-";
    }
}

ChunkedTerrainRenderer* TerrainBrush::FindTerrain() const
{
    const GameObject* owner = GetOwner();
    return owner ? owner->GetComponent<ChunkedTerrainRenderer>() : nullptr;
}

void TerrainBrush::Update()
{
    ChunkedTerrainRenderer* terrain = FindTerrain();
    if (!m_graphics || !terrain || !terrain->IsEditable())
        return;

    const InputManager& input = InputManager::Get();
    const float deltaTime = (std::min)(TimeManager::Get().GetDeltaTime(), 0.1f);

    // ---- 도구 ----
    if (input.GetKeyDown('1')) m_tool = Tool::Raise;
    if (input.GetKeyDown('2')) m_tool = Tool::Lower;
    if (input.GetKeyDown('3')) m_tool = Tool::Flatten;
    if (input.GetKeyDown('4')) m_tool = Tool::Smooth;

    // 우클릭은 카메라 프리룩이 쓴다. 그동안 브러시는 쉰다.
    const bool cameraLooking = input.GetMouseButton(InputManager::Right);

    // ---- 반경 (휠) / 세기 ([ ]) ----
    if (!cameraLooking)
    {
        if (const int wheel = input.GetMouseWheelDelta(); wheel != 0)
        {
            const float notches = static_cast<float>(wheel) / WHEEL_DELTA;
            m_radius = (std::max)(kMinRadius, (std::min)(kMaxRadius, m_radius * std::pow(1.12f, notches)));
        }
    }

    if (input.GetKeyDown(VK_OEM_4)) m_strength = (std::max)(kMinStrength, m_strength / 1.25f);   // [
    if (input.GetKeyDown(VK_OEM_6)) m_strength = (std::min)(kMaxStrength, m_strength * 1.25f);   // ]

    // ---- 피킹 (S67) ----
    //  에디터 패널 위에 있는 마우스는 UI 몫이다.
    m_hasHit = false;
    if (!cameraLooking && !input.IsPointerOverUI())
    {
        XMFLOAT3 origin{};
        XMFLOAT3 direction{};
        if (m_graphics->ScreenToRay(input.GetMouseX(), input.GetMouseY(), origin, direction))
            m_hasHit = ground::Raycast(*terrain, origin, direction, kMaxPickDistance, m_hitPoint);
    }

    // ---- 칠하기 ----
    //  누르기 시작한 곳이 지형이어야 칠한다. 패널에서 누르고 끌고 나온 것은 칠하지 않는다.
    if (input.GetMouseButtonDown(InputManager::Left) && m_hasHit)
    {
        m_painting = true;
        m_flattenHeight = m_hitPoint.y;   // 평탄화는 누른 순간의 높이에 맞춘다
    }

    if (!input.GetMouseButton(InputManager::Left))
        m_painting = false;

    if (m_painting && m_hasHit)
        Apply(*terrain, deltaTime);

    terrain->SetBrushPreview(m_hasHit, m_hitPoint.x, m_hitPoint.z, m_radius, static_cast<int>(m_tool));
}

// -------------------------------------------------------------
// 격자 값 고치기
// -------------------------------------------------------------
void TerrainBrush::Apply(ChunkedTerrainRenderer& terrain, float deltaTime)
{
    terrain::HeightGrid* grid = terrain.GetEditGrid();
    if (!grid || !grid->IsValid())
        return;

    const int lastColumn = grid->GetColumns() - 1;
    const int lastRow = grid->GetRows() - 1;

    // 원을 감싸는 격자 사각형. 행은 -Z 로 커지므로 z 가 큰 쪽이 첫 행이다.
    const int column0 = (std::max)(0, static_cast<int>(std::floor(grid->ToColumn(m_hitPoint.x - m_radius))));
    const int column1 = (std::min)(lastColumn, static_cast<int>(std::ceil(grid->ToColumn(m_hitPoint.x + m_radius))));
    const int row0 = (std::max)(0, static_cast<int>(std::floor(grid->ToRow(m_hitPoint.z + m_radius))));
    const int row1 = (std::min)(lastRow, static_cast<int>(std::ceil(grid->ToRow(m_hitPoint.z - m_radius))));

    if (column0 > column1 || row0 > row1)
        return;

    // 부드럽게 : 고치는 도중의 값을 이웃 평균에 쓰면 칠하는 방향으로 쏠린다.
    //  그래서 한 칸 테두리까지 원본을 먼저 복사해 두고 거기서 평균을 낸다.
    const int copyWidth = column1 - column0 + 3;
    std::vector<float> original;
    if (m_tool == Tool::Smooth)
    {
        original.resize(static_cast<size_t>(copyWidth) * (row1 - row0 + 3));
        for (int row = row0 - 1; row <= row1 + 1; ++row)
            for (int column = column0 - 1; column <= column1 + 1; ++column)
                original[static_cast<size_t>(row - row0 + 1) * copyWidth + (column - column0 + 1)] = grid->Get(column, row);
    }

    const float rate = m_strength * deltaTime;

    for (int row = row0; row <= row1; ++row)
    {
        for (int column = column0; column <= column1; ++column)
        {
            const float dx = grid->ColumnToX(column) - m_hitPoint.x;
            const float dz = grid->RowToZ(row) - m_hitPoint.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (distance >= m_radius)
                continue;

            // 감쇠 : 가운데 1 → 가장자리 0 (smoothstep)
            const float t = 1.0f - distance / m_radius;
            const float weight = t * t * (3.0f - 2.0f * t);

            float height = grid->Get(column, row);

            switch (m_tool)
            {
            case Tool::Raise:
                height += rate * weight;
                break;

            case Tool::Lower:
                height -= rate * weight;
                break;

            case Tool::Flatten:
                height += (m_flattenHeight - height) * (std::min)(1.0f, rate * 0.25f * weight);
                break;

            case Tool::Smooth:
            {
                float sum = 0.0f;
                for (int oz = -1; oz <= 1; ++oz)
                    for (int ox = -1; ox <= 1; ++ox)
                        sum += original[static_cast<size_t>(row - row0 + 1 + oz) * copyWidth + (column - column0 + 1 + ox)];

                height += (sum / 9.0f - height) * (std::min)(1.0f, rate * 0.25f * weight);
                break;
            }

            default:
                break;
            }

            grid->Set(column, row, height);
        }
    }

    // 바뀐 영역만 알린다. 지형은 이 영역과 겹치는 청크만 다시 만든다.
    terrain.MarkRegionDirty(m_hitPoint.x - m_radius, m_hitPoint.z - m_radius,
                            m_hitPoint.x + m_radius, m_hitPoint.z + m_radius);
}

void TerrainBrush::ToJson(json::Value& out) const
{
    Component::ToJson(out);
    out["tool"]     = json::Value(static_cast<int>(m_tool));
    out["radius"]   = json::Value(m_radius);
    out["strength"] = json::Value(m_strength);
}

void TerrainBrush::FromJson(const json::Value& in)
{
    Component::FromJson(in);

    if (const json::Value* value = in.Find("tool"))
    {
        const int tool = value->AsInt(0);
        if (tool >= 0 && tool < static_cast<int>(Tool::Count))
            m_tool = static_cast<Tool>(tool);
    }
    if (const json::Value* value = in.Find("radius"))   m_radius   = value->AsFloat(m_radius);
    if (const json::Value* value = in.Find("strength")) m_strength = value->AsFloat(m_strength);
}
