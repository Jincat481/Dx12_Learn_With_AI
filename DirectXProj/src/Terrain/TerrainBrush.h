#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Terrain/HydraulicErosion.h"

class Graphics;
class ChunkedTerrainRenderer;

// =============================================================
// TerrainBrush (S68)
//  마우스로 지형을 칠해 높이를 고친다. 지형 오브젝트에 붙이는 컴포넌트다.
//
//  매 프레임
//   1) 마우스 레이로 지형 위의 점을 찾는다 (S67)
//   2) 왼쪽 버튼을 누르고 있으면 그 점 주변 격자 값을 고친다
//   3) 바뀐 영역을 지형에 알려, 겹치는 청크만 다시 만들게 한다 (S69)
//
//  도구
//   1 올리기   : 가운데일수록 많이 올린다
//   2 내리기   : 가운데일수록 많이 내린다
//   3 평탄화   : 누른 순간의 높이로 서서히 맞춘다
//   4 부드럽게 : 이웃 평균으로 서서히 당긴다 (뾰족한 곳이 무뎌진다)
//   5 침식     : 원 안에만 물방울을 떨어뜨려 그 자리에 물길을 낸다 (S74)
//
//  감쇠(falloff)
//   원 안 모든 점을 같은 양만큼 올리면 경계가 절벽이 된다.
//   중심에서 1, 가장자리에서 0 인 smoothstep 곡선을 곱해 경계를 매끄럽게 한다.
// =============================================================
class TerrainBrush : public Component
{
public:
    enum class Tool { Raise, Lower, Flatten, Smooth, Erode, Count };

    TerrainBrush() = default;
    ~TerrainBrush() override = default;

    const char* GetTypeName() const override { return "TerrainBrush"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;

    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

    Tool  GetTool() const { return m_tool; }
    const wchar_t* GetToolName() const;
    float GetRadius() const { return m_radius; }
    float GetStrength() const { return m_strength; }

    bool HasHit() const { return m_hasHit; }
    const DirectX::XMFLOAT3& GetHitPoint() const { return m_hitPoint; }

private:
    ChunkedTerrainRenderer* FindTerrain() const;
    void Apply(ChunkedTerrainRenderer& terrain, float deltaTime);

    Graphics* m_graphics = nullptr;

    Tool  m_tool = Tool::Raise;
    float m_radius = 24.0f;       // 월드 단위
    float m_strength = 12.0f;     // 올리기/내리기 : 초당 높이 변화량 (가운데 기준)

    bool  m_hasHit = false;
    DirectX::XMFLOAT3 m_hitPoint{ 0.0f, 0.0f, 0.0f };

    bool  m_painting = false;

    terrain::HydraulicErosion m_erosion;
    std::mt19937 m_rng{ 777u };
    float m_flattenHeight = 0.0f;

    static constexpr float kMinRadius = 4.0f;
    static constexpr float kMaxRadius = 160.0f;
    static constexpr float kMinStrength = 1.0f;
    static constexpr float kMaxStrength = 80.0f;
    static constexpr float kMaxPickDistance = 2000.0f;
};
