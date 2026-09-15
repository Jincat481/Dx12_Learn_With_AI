#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Mesh.h"
#include "Engine/WaterRipples.h"
#include "Engine/OceanWaves.h"

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
//
//  바다 (S85~S87) : 수면은 카메라를 따라다니는 촘촘한 격자다. OceanWaves 가 바람 스펙트럼으로
//  계산한 파도 텍스처를 정점 셰이더가 읽어 실제로 들어 올린다.
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

    void SetRainEnabled(bool enabled) { m_rain = enabled; }
    bool IsRainEnabled() const { return m_rain; }
    void ToggleRain() { m_rain = !m_rain; }

    // 디버그 : 물결 높이를 색으로 (빨강 = 마루, 파랑 = 골)
    bool IsRippleDebugEnabled() const { return m_rippleDebug; }
    void ToggleRippleDebug() { m_rippleDebug = !m_rippleDebug; }

    // ---- 진폭 보기 (S82) ----
    //  물결 높이 텍스처를 화면 오른쪽에 그대로 띄우고, 값을 읽어 와 숫자와 그래프로 보여 준다.
    bool IsAmplitudeViewEnabled() const { return m_amplitudeView; }
    void ToggleAmplitudeView();

    // ---- 바다 파도 (S85~S87) ----
    void  CycleWindSpeed();                       // 4 → 8 → 13 → 18 m/s
    void  RotateWind(float degrees);
    void  ToggleChoppy();                         // 뾰족한 마루(게르스트너) ↔ 둥근 사인파
    bool  IsChoppy() const { return m_ocean.GetChoppiness() > 0.01f; }
    float GetWindSpeed() const { return m_ocean.GetWindSpeed(); }
    float GetWindDirection() const { return m_ocean.GetWindDirection(); }
    float GetSignificantWaveHeight() const { return m_ocean.GetSignificantHeight(); }
    float GetPeakWavelength() const { return m_ocean.GetPeakWavelength(); }

    // GDI 오버레이 단계에서 Game 이 부른다. (텍스처 창은 RenderTransparent 에서 D3D 로 먼저 그린다)
    void DrawAmplitudeOverlay(HDC hdc, int viewportWidth, int viewportHeight) const;

    // 해안 마스크를 굽는 중인가 (S81)
    bool IsBuildingShore() const { return m_shoreRow >= 0; }
    bool HasShore() const { return m_hasShore; }

private:
    bool PickWaterSurface(int mouseX, int mouseY, DirectX::XMFLOAT3& outHit) const;
    void AddRainDrops();
    void UpdateShoreMask();
    bool BuildOceanGrid();

    struct AmplitudeLayout
    {
        RECT panel;
        RECT texture;
        RECT text;
        RECT profile;
        RECT history;
    };
    static AmplitudeLayout ComputeAmplitudeLayout(int viewportWidth, int viewportHeight);

    bool IsInsideAmplitudePanel(int x, int y, bool* insideTexture) const;
    bool PickFromAmplitudeView(int x, int y, DirectX::XMFLOAT3& outHit) const;
    void UpdateAmplitudeView(float deltaTime);
    void AnalyzeReadback(bool freshData);

    Graphics* m_graphics = nullptr;
    Mesh      m_plane;          // 바다 격자 (S85)
    OceanWaves m_ocean;

    // 클릭 물결 (S79)
    WaterRipples m_ripples;
    bool  m_dragging = false;
    DirectX::XMFLOAT3 m_lastDrop{ 0.0f, 0.0f, 0.0f };
    float m_stepAccumulator = 0.0f;
    std::mt19937 m_rng{ 4242u };

    bool  m_rain = false;
    bool  m_rippleDebug = false;

    // 해안 높이 맵 (S81, S84) : 물결 영역을 128 x 128 로 나눠 "땅 높이 - 수위" 를 적는다. 양수면 땅(벽).
    //  한 번에 다 구하면 수천 번의 지면 질의로 프레임이 멈추므로 몇 줄씩 나눠 굽는다.
    //  다 구운 값은 GPU(수면 잠재우기)와 파동 입자(해안 반사, S89)가 함께 쓴다.
    ComPtr<ID3D11Texture2D>          m_shoreTexture;
    ComPtr<ID3D11ShaderResourceView> m_shoreView;
    std::vector<float>               m_shoreBuilding;   // 땅 높이 - 수위 (양수 땅, 음수 물 깊이)
    std::vector<const IGroundProvider*> m_shoreProviders;
    DirectX::XMFLOAT3 m_shoreBuildRegion{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 m_shoreRegion{ 0.0f, 0.0f, 1.0f };
    float m_shoreBuildLevel = 0.0f;
    float m_shoreLevel = 0.0f;
    int   m_shoreRow = -1;       // 굽는 중인 줄 (-1 이면 쉬는 중)
    bool  m_hasShore = false;

    // 진폭 보기 (S82)
    bool  m_amplitudeView = false;
    float m_readbackTimer = 0.0f;
    std::vector<float> m_readback;                 // 마지막으로 읽어 온 높이 (size x size)
    DirectX::XMFLOAT3  m_readbackRegion{ 0.0f, 0.0f, 1.0f };
    bool  m_hasReadback = false;
    float m_maxAmplitude = 0.0f;
    float m_rmsAmplitude = 0.0f;
    int   m_maxColumn = 0;
    int   m_maxRow = 0;
    bool  m_probeValid = false;                    // 커서가 수면이나 텍스처 창 위에 있는가
    DirectX::XMFLOAT3 m_probeWorld{ 0.0f, 0.0f, 0.0f };
    float m_probeValue = 0.0f;
    int   m_profileRow = -1;                       // 단면 그래프가 읽는 줄
    int   m_profileColumn = -1;                    // 단면 그래프에 세로선을 그을 칸
    bool  m_profileFromCursor = false;
    std::vector<float> m_profile;
    std::vector<float> m_history;                  // 최대 진폭 기록 (0.1초마다)

    static constexpr int   kViewSize = 256;
    static constexpr int   kHistorySize = 80;
    static constexpr float kReadbackInterval = 0.1f;

    static constexpr int kShoreSize = 128;
    static constexpr int kShoreRowsPerFrame = 6;

    // 입자는 한 단계에 일정 거리를 움직이고 부딪힘도 단계 단위로 깎는다. 프레임과 무관하게 초당 60 단계.
    static constexpr float kStepSeconds = 1.0f / 60.0f;
    static constexpr int   kMaxStepsPerFrame = 4;

    float m_level = 0.0f;
    float m_time = 0.0f;
    bool  m_reflection = true;
    bool  m_refraction = true;
    bool  m_foam = true;

    static constexpr float kHalfSize = 2000.0f;   // far 평면(1000)보다 넉넉히
    static constexpr int   kGridSize = 256;       // 격자 한 변의 정점 수
    static constexpr float kNearExtent = 40.0f;   // 가운데 간격 = 이 값 · 2 / (정점 수 - 1) ≈ 0.31 m
};
