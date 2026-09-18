#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Mesh.h"

#include <random>
#include <vector>

class Graphics;
class IGroundProvider;
class WaterRenderer;

// =============================================================
// WaterfallRenderer (S90)
//  지형 위로 물 한 줄기를 흘려 폭포 · 계류를 만든다.
//
//  1) 자리 찾기 : 카메라 주변을 성기게 훑어 "물 위로 높고 경사가 급한" 후보를 고르고,
//                 각 후보에서 실제로 물을 흘려 본 뒤 낙차가 가장 큰 물길을 고른다.
//  2) 흘리기   : 물 한 덩이를 중력으로 굴린다.
//                 - 지면을 탈 때 : 중력을 경사면에 투영한 만큼 빨라지고, 마찰로 느려진다
//                 - 지면이 발밑에서 꺼지면(경사가 낙하보다 가파르면) 공중으로 떨어진다 → 이 구간이 폭포다
//                 - 다시 지면에 닿으면 튀며 속도를 잃고 계속 흐른다
//                 - 수면에 닿으면 끝난다
//  3) 리본 만들기 : 지나온 점마다 진행 방향과 직각으로 좌우 정점을 놓아 띠 모양 메시를 만든다.
//                   정점에 속도 · 공중 여부 · 낙하 거리 · 흐른 거리를 실어 셰이더가 무늬와 부서짐에 쓴다.
//
//  떨어지는 자리가 바다면 그 자리에 계속 물방울을 떨어뜨려 물결(S89)을 일으킨다.
//
//  지형이 바뀌면(새 지형 N, 브러시, 침식) 발원지 높이가 달라지므로 물길을 다시 찾는다.
// =============================================================
class WaterfallRenderer : public Component
{
public:
    WaterfallRenderer() = default;
    ~WaterfallRenderer() override = default;

    const char* GetTypeName() const override { return "WaterfallRenderer"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;
    void RenderTransparent() override;
    void OnDestroy() override;

    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

    // 물길을 찾을 범위 (xz 중심과 반지름)
    void SetSearchArea(float centerX, float centerZ, float radius);

    void  SetWaterLevel(float level) { m_waterLevel = level; }
    float GetWaterLevel() const { return m_waterLevel; }

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    void Toggle() { m_enabled = !m_enabled; }

    void ToggleDebug() { m_debug = !m_debug; }
    bool IsDebugEnabled() const { return m_debug; }

    // 지금 지형에서 물길을 다시 찾는다
    void Rebuild();

    // 다음으로 좋은 자리로 옮긴다 (후보를 순서대로 돌려 본다)
    void NextSite();

    // 폭포가 잘 보이는 카메라 자리와 바라볼 점 (8 키)
    bool GetViewpoint(DirectX::XMFLOAT3& outEye, DirectX::XMFLOAT3& outTarget) const;

    bool  HasPath() const { return m_mesh.IsValid(); }
    float GetDropHeight() const { return m_dropHeight; }    // 발원지 ~ 끝 높이 차
    float GetFallHeight() const { return m_fallHeight; }    // 그중 공중으로 떨어진 높이
    float GetPathLength() const { return m_pathLength; }
    int   GetNodeCount() const { return static_cast<int>(m_nodes.size()); }

private:
    // 물줄기가 지나간 한 점
    struct Node
    {
        DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 flow{ 0.0f, 0.0f, 1.0f };     // 진행 방향
        DirectX::XMFLOAT3 side{ 1.0f, 0.0f, 0.0f };     // 물줄기를 가로지르는 방향
        float speed = 0.0f;
        float width = 3.0f;
        float distance = 0.0f;   // 발원지에서 흐른 거리
        float fallen = 0.0f;     // 지금 낙하에서 떨어진 거리 (지면을 타면 0)
        bool  airborne = false;
    };

    void  CollectProviders();
    bool  SampleGround(float x, float z, float& outHeight) const;
    DirectX::XMFLOAT3 GroundNormal(float x, float z) const;

    bool  FindSource(DirectX::XMFLOAT3& outSource) const;
    bool  TracePath(const DirectX::XMFLOAT3& source, std::vector<Node>& outNodes) const;
    void  BuildMesh();
    void  SpawnSplashes(float deltaTime);

    Graphics* m_graphics = nullptr;
    Mesh      m_mesh;

    std::vector<const IGroundProvider*> m_providers;
    std::vector<Node> m_nodes;

    // 물이 떨어지며 수면을 때리는 자리 (물결을 일으킨다)
    std::vector<DirectX::XMFLOAT3> m_splashPoints;

    // 물보라가 피어오르는 자리 (바닥을 때린 곳). 같은 메시에 빌보드 사각형으로 넣는다.
    std::vector<DirectX::XMFLOAT3> m_mistPoints;
    WaterRenderer* m_water = nullptr;
    std::mt19937 m_rng{ 1234u };
    float m_splashTimer = 0.0f;

    float m_centerX = 0.0f;
    float m_centerZ = -40.0f;
    float m_radius = 260.0f;
    float m_waterLevel = 0.0f;

    float m_time = 0.0f;
    float m_checkTimer = 0.0f;
    float m_sourceHeight = 0.0f;      // 마지막으로 만들 때의 발원지 높이 (지형이 바뀌었는지 본다)
    bool  m_needsRebuild = true;
    bool  m_enabled = true;
    bool  m_debug = false;

    int   m_choice = 0;               // 몇 번째로 좋은 자리를 쓸지 (7 키로 바꾼다)
    float m_dropHeight = 0.0f;
    float m_fallHeight = 0.0f;
    float m_pathLength = 0.0f;

    // ---- 흘리기 상수 ----
    static constexpr float kStepSeconds = 0.05f;
    static constexpr float kGravity = 9.81f;
    static constexpr float kFriction = 0.40f;      // 지면을 탈 때의 마찰 (1/초)
    static constexpr float kMaxSpeed = 22.0f;
    // 지면이 이만큼 꺼지면 물이 지면을 떠난다.
    //  한 단계에 0.5 m 쯤 나아가므로 0.15 m 는 약 17도보다 가파른 "꺾임" 에서 물이 뜬다는 뜻이다.
    //  노이즈 지형에는 수직 절벽이 거의 없어, 이보다 크게 잡으면 공중 구간이 아예 생기지 않는다.
    static constexpr float kAirGap = 0.15f;
    static constexpr int   kMaxSteps = 600;
    static constexpr float kBaseWidth = 4.5f;
    static constexpr float kMaxWidth = 13.0f;
    static constexpr float kTextureLength = 9.0f;  // 무늬 한 장이 덮는 길이(m)
    static constexpr float kLift = 0.35f;          // 지면에서 살짝 띄운다 (z-파이팅 방지)
    static constexpr float kSplashInterval = 0.09f;
    static constexpr size_t kMaxCandidates = 10;   // 실제로 물을 흘려 볼 후보 수
    static constexpr int   kPuffsPerPoint = 14;    // 떨어지는 자리마다 띄우는 물보라 수
};
