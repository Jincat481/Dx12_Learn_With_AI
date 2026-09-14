#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Mesh.h"

class Graphics;

// =============================================================
// SkyRenderer (스텝 8, 9 / S54~S57)
//  하늘을 그린다.
//
//  스카이박스 vs 스카이돔
//   - 스카이박스 : 정육면체 6면에 큐브맵을 붙인다. 텍스처가 필요하다.
//   - 스카이돔   : 구(반구)를 씌우고 방향에 따라 색을 계산한다.
//  여기서는 돔을 쓴다. 텍스처 없이 지평선~천정 그라데이션을 만들 수 있고,
//  스텝 9 의 구름을 그 위에 바로 얹을 수 있기 때문이다.
//
//  핵심 규칙
//   1) 돔은 매 프레임 카메라 위치로 따라다닌다. 그래야 아무리 걸어도
//      하늘에 가까워지지 않는다(무한히 멀리 있는 것처럼 보인다).
//   2) 깊이 버퍼에 쓰지 않는다. 하늘은 항상 모든 것의 뒤에 있어야 한다.
// =============================================================
class SkyRenderer : public Component
{
public:
    SkyRenderer() = default;
    ~SkyRenderer() override = default;

    const char* GetTypeName() const override { return "SkyRenderer"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;
    void Render() override;
    void OnDestroy() override;

    // ---- 스텝 9 : 구름 ----
    void SetCloudsEnabled(bool enabled) { m_cloudsEnabled = enabled; }
    bool AreCloudsEnabled() const { return m_cloudsEnabled; }
    void ToggleClouds() { m_cloudsEnabled = !m_cloudsEnabled; }

    void SetCloudCoverage(float coverage) { m_cloudCoverage = coverage; }
    float GetCloudCoverage() const { return m_cloudCoverage; }

    void SetCloudSpeed(float speed) { m_cloudSpeed = speed; }
    void SetSunDirection(const DirectX::XMFLOAT3& direction);

    // ---- 보강 : 거리 안개 / 대기 원근 (S64) ----
    //  안개 색은 하늘의 지평선 색을 그대로 쓴다. 그래야 지형 끝이 하늘에 녹아든다.
    void SetFogEnabled(bool enabled) { m_fogEnabled = enabled; }
    bool IsFogEnabled() const { return m_fogEnabled; }
    void ToggleFog() { m_fogEnabled = !m_fogEnabled; }

    // start 부터 옅어지기 시작해 end 에서 완전히 덮는다. density 는 지수 안개의 짙기.
    void SetFogRange(float start, float end, float density);

private:
    bool BuildDome(int slices, int stacks, float radius);

    Graphics* m_graphics = nullptr;
    Mesh m_dome;

    DirectX::XMFLOAT3 m_sunDirection{ -0.45f, -1.0f, 0.35f };

    DirectX::XMFLOAT4 m_horizonColor{ 0.62f, 0.72f, 0.86f, 1.0f };
    DirectX::XMFLOAT4 m_zenithColor{ 0.12f, 0.30f, 0.62f, 1.0f };

    bool  m_cloudsEnabled = false;
    float m_cloudCoverage = 0.48f;
    float m_cloudSpeed = 0.015f;
    float m_time = 0.0f;

    bool  m_fogEnabled = true;
    float m_fogStart = 150.0f;
    float m_fogEnd = 900.0f;
    float m_fogDensity = 0.0018f;
    float m_sunScatter = 0.45f;     // SkyPS 의 태양 번짐 세기와 같은 값
};
