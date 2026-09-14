#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Mesh.h"

class Graphics;

// =============================================================
// WaterRenderer (S76~S78)
//  수면 하나를 그린다. 판은 카메라를 따라다니고 높이는 수위에 고정된다.
//
//  물은 "뒤에 있는 것" 을 알아야 그릴 수 있다.
//   - 반사 : 물 위의 세상을 수면 기준으로 뒤집어 한 번 더 그린다 (Game 이 반사 패스를 먼저 돌린다)
//   - 굴절 : 불투명한 것을 다 그린 뒤의 화면과 깊이를 복사해 읽는다
//  그래서 일반 Render 가 아니라 RenderTransparent 단계에서 그린다.
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

private:
    Graphics* m_graphics = nullptr;
    Mesh      m_plane;

    float m_level = 0.0f;
    float m_time = 0.0f;
    bool  m_reflection = true;
    bool  m_refraction = true;
    bool  m_foam = true;

    static constexpr float kHalfSize = 2000.0f;   // far 평면(1000)보다 넉넉히
};
