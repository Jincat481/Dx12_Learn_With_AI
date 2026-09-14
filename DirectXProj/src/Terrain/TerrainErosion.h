#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Terrain/HydraulicErosion.h"

// =============================================================
// TerrainErosion (S74)
//  편집 가능한 청크 지형에 붙어, 켜 두면 매 프레임 물방울을 떨어뜨린다.
//
//  R 을 누르면 정해진 수(kDropletsPerRun)만큼 떨어뜨리고 스스로 멈춘다.
//  끝없이 돌리면 흙이 가장자리로 계속 쓸려 나가 지형 전체가 평평해지기 때문이다.
//  다시 R 을 누르면 한 번 더 돌린다.
//
//  한 프레임에 몇 방울을 떨어뜨릴지는 개수가 아니라 "시간 예산" 으로 정한다.
//  빌드(Debug/Release)나 컴퓨터마다 속도가 달라도 화면이 멈추지 않게 하려는 것이다.
//  바뀐 영역은 지형에 알리고, 지형이 청크를 다시 만든다(많으면 작업 스레드로).
// =============================================================
class TerrainErosion : public Component
{
public:
    TerrainErosion() = default;
    ~TerrainErosion() override = default;

    const char* GetTypeName() const override { return "TerrainErosion"; }

    void Update() override;

    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

    void SetRunning(bool running);
    bool IsRunning() const { return m_running; }
    void ToggleRunning() { SetRunning(!m_running); }

    int GetRunDroplets() const { return m_runDroplets; }
    static constexpr int kDropletsPerRun = 400000;   // 격자점 하나당 약 6방울

    uint64_t GetTotalDroplets() const { return m_totalDroplets; }
    int      GetDropletsLastFrame() const { return m_dropletsLastFrame; }
    float    GetLastFrameMs() const { return m_lastFrameMs; }

private:
    terrain::HydraulicErosion m_erosion;
    std::mt19937 m_rng{ 20240914u };

    bool     m_running = false;
    float    m_budgetMs = 4.0f;        // 한 프레임에 침식 계산에 쓸 시간
    int      m_runDroplets = 0;        // 이번 실행에서 떨어뜨린 수
    uint64_t m_totalDroplets = 0;
    int      m_dropletsLastFrame = 0;
    float    m_lastFrameMs = 0.0f;

    static constexpr int kDropletBatch = 64;   // 시간을 이만큼마다 확인한다
};
