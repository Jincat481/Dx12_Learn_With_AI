#include "Core/stdafx.h"
#include "Terrain/TerrainErosion.h"
#include "Terrain/ChunkedTerrainRenderer.h"
#include "Terrain/HeightGrid.h"
#include "Engine/GameObject.h"
#include "Input/InputManager.h"

#include <chrono>

void TerrainErosion::SetRunning(bool running)
{
    // 새로 시작할 때마다 이번 실행의 개수를 0 부터 센다.
    if (running && !m_running)
        m_runDroplets = 0;

    m_running = running;
}

void TerrainErosion::Update()
{
    const InputManager& input = InputManager::Get();

    if (input.GetKeyDown('R'))
    {
        ToggleRunning();
        dxutil::DebugLog(L"[Erosion] %s", m_running ? L"침식 시작" : L"침식 멈춤");
    }

    m_dropletsLastFrame = 0;
    m_lastFrameMs = 0.0f;

    if (!m_running)
        return;

    const GameObject* owner = GetOwner();
    ChunkedTerrainRenderer* terrain = owner ? owner->GetComponent<ChunkedTerrainRenderer>() : nullptr;
    terrain::HeightGrid* grid = terrain ? terrain->GetEditGrid() : nullptr;

    // 침식은 격자 값을 고친다. 함수(노이즈) 지형에는 고칠 값이 없다.
    if (!grid || !grid->IsValid())
        return;

    const auto start = std::chrono::steady_clock::now();
    terrain::GridBounds touched;

    // 시간 예산을 다 쓸 때까지 64방울씩 떨어뜨린다.
    float elapsedMs = 0.0f;
    do
    {
        m_erosion.Simulate(*grid, kDropletBatch, m_rng, touched);
        m_dropletsLastFrame += kDropletBatch;

        elapsedMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    while (elapsedMs < m_budgetMs && m_runDroplets + m_dropletsLastFrame < kDropletsPerRun);

    m_totalDroplets += static_cast<uint64_t>(m_dropletsLastFrame);
    m_runDroplets += m_dropletsLastFrame;
    m_lastFrameMs = elapsedMs;

    if (m_runDroplets >= kDropletsPerRun)
    {
        m_running = false;
        dxutil::DebugLog(L"[Erosion] %d 방울을 다 떨어뜨려 멈춘다", m_runDroplets);
    }

    if (!touched.IsEmpty())
    {
        // 격자 좌표 → 월드. 행은 -Z 로 커지므로 minRow 가 z 의 최댓값이다.
        terrain->MarkRegionDirty(grid->ColumnToX(touched.minColumn), grid->RowToZ(touched.maxRow),
                                 grid->ColumnToX(touched.maxColumn), grid->RowToZ(touched.minRow));
    }
}

void TerrainErosion::ToJson(json::Value& out) const
{
    Component::ToJson(out);
    out["running"]  = json::Value(m_running);
    out["budgetMs"] = json::Value(m_budgetMs);
}

void TerrainErosion::FromJson(const json::Value& in)
{
    Component::FromJson(in);
    if (const json::Value* value = in.Find("running"))  m_running  = value->AsBool(m_running);
    if (const json::Value* value = in.Find("budgetMs")) m_budgetMs = value->AsFloat(m_budgetMs);
}
