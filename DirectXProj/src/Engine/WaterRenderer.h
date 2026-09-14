#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Mesh.h"
#include "Engine/WaterRipples.h"

#include <random>

class IGroundProvider;

class Graphics;

// =============================================================
// WaterRenderer (S76~S78)
//  수면 하나를 그린다. 판은 카메라를 따라다니고 높이는 수위에 고정된다.
//
//  물은 "뒤에 있는 것" 을 알아야 그릴 수 있다.
//   - 반사 : 물 위의 세상을 수면 기준으로 뒤집어 한 번 더 그린다 (Game 이 반사 패스를 먼저 돌린다)
//   - 굴절 : 불투명한 것을 다 그린 뒤의 화면과 깊이를 복사해 읽는다
//  그래서 일반 Render 가 아니라 RenderTransparent 단계에서 그린다.
//
//  클릭 물결 (S79) : 마우스 레이와 수면의 교점에 물방울을 떨어뜨리고, 렌더 타깃 핑퐁으로 퍼뜨린다.
//  자연스러운 흐름 (S80) : 컬 노이즈 해류를 따라 잔물결 무늬를 흘려보낸다 (셰이더).
// =============================================================
class WaterRenderer : public Component
{
public:
    WaterRenderer() = default;
    ~WaterRenderer() override = default;

    const char* GetTypeName() const override { return "WaterRenderer"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;
    void RenderTransparent() override;
    void OnDestroy() override;

    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

    void  SetWaterLevel(float level) { m_level = level; }
    float GetWaterLevel() const { return m_level; }

    void SetReflectionEnabled(bool enabled) { m_reflection = enabled; }
    bool IsReflectionEnabled() const { return m_reflection; }
    void ToggleReflection() { m_reflection = !m_reflection; }

    void SetRefractionEnabled(bool enabled) { m_refraction = enabled; }
    bool IsRefractionEnabled() const { return m_refraction; }
    void ToggleRefraction() { m_refraction = !m_refraction; }

    void SetFlowEnabled(bool enabled) { m_flow = enabled; }
    bool IsFlowEnabled() const { return m_flow; }
    void ToggleFlow() { m_flow = !m_flow; }

    void SetRainEnabled(bool enabled) { m_rain = enabled; }
    bool IsRainEnabled() const { return m_rain; }
    void ToggleRain() { m_rain = !m_rain; }

    // 디버그 : 물결 높이를 색으로 (빨강 = 마루, 파랑 = 골)
    bool IsRippleDebugEnabled() const { return m_rippleDebug; }
    void ToggleRippleDebug() { m_rippleDebug = !m_rippleDebug; }

    // 해안 마스크를 굽는 중인가 (S81)
    bool IsBuildingShore() const { return m_shoreRow >= 0; }
    bool HasShore() const { return m_hasShore; }

private:
    bool PickWaterSurface(int mouseX, int mouseY, DirectX::XMFLOAT3& outHit) const;
    void AddRainDrops();
    void UpdateShoreMask();

    Graphics* m_graphics = nullptr;
    Mesh      m_plane;

    // 클릭 물결 (S79)
    WaterRipples m_ripples;
    bool  m_dragging = false;
    DirectX::XMFLOAT3 m_lastDrop{ 0.0f, 0.0f, 0.0f };
    float m_stepAccumulator = 0.0f;
    std::mt19937 m_rng{ 4242u };

    bool  m_flow = true;
    bool  m_rain = false;
    bool  m_rippleDebug = false;

    // 해안 마스크 (S81) : 물결 영역을 128 x 128 로 나눠 땅이면 1 을 적는다.
    //  한 번에 다 구하면 수천 번의 지면 질의로 프레임이 멈추므로 몇 줄씩 나눠 굽는다.
    ComPtr<ID3D11Texture2D>          m_shoreTexture;
    ComPtr<ID3D11ShaderResourceView> m_shoreView;
    std::vector<uint8_t>             m_shoreBuilding;
    std::vector<const IGroundProvider*> m_shoreProviders;
    DirectX::XMFLOAT3 m_shoreBuildRegion{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 m_shoreRegion{ 0.0f, 0.0f, 1.0f };
    float m_shoreBuildLevel = 0.0f;
    float m_shoreLevel = 0.0f;
    int   m_shoreRow = -1;       // 굽는 중인 줄 (-1 이면 쉬는 중)
    bool  m_hasShore = false;

    static constexpr int kShoreSize = 128;
    static constexpr int kShoreRowsPerFrame = 6;

    // 파동 방정식은 한 단계의 시간 간격이 일정해야 퍼지는 속도가 일정하다. 프레임과 무관하게 초당 60 단계.
    static constexpr float kStepSeconds = 1.0f / 60.0f;
    static constexpr int   kMaxStepsPerFrame = 4;

    float m_level = 0.0f;
    float m_time = 0.0f;
    bool  m_reflection = true;
    bool  m_refraction = true;
    bool  m_foam = true;

    static constexpr float kHalfSize = 2000.0f;   // far 평면(1000)보다 넉넉히
};
