#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"

class Graphics;

// =============================================================
// Camera (S27, S28)
//  터레인을 둘러보기 위한 궤도(orbit) 카메라.
//
//  타깃 한 점을 중심으로 yaw(좌우) / pitch(위아래) 각도와 거리로
//  눈 위치를 정하고, LookAt 으로 뷰 행렬을 만든다.
//      eye = target + (거리 × 회전된 방향)
//
//  Update 에서 계산한 View / Projection 을 Graphics 에 넘겨 두면
//  같은 프레임의 Render 단계에서 메시가 그것을 쓴다.
//
//  조작
//    마우스 가운데 버튼 드래그 : 궤도 회전
//    마우스 휠                : 줌
//    WASD                    : 타깃 수평 이동
//    Q / E                   : 타깃 높이
//    F                       : 원점으로 리셋
// =============================================================
class Camera : public Component
{
public:
    Camera() = default;
    ~Camera() override = default;

    const char* GetTypeName() const override { return "Camera"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;

    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

    void SetTarget(const DirectX::XMFLOAT3& target) { m_target = target; }
    void SetDistance(float distance) { m_distance = distance; }
    void SetAngles(float yawDeg, float pitchDeg) { m_yaw = yawDeg; m_pitch = pitchDeg; }

    DirectX::XMFLOAT3 GetEyePosition() const;

private:
    void ApplyToGraphics();

    // 궤도 파라미터로 계산한 눈 위치·회전을 소유 GameObject 의 Transform 에 기록한다.
    // 그래야 Inspector 에 실제 카메라 위치가 보인다.
    void WriteToTransform();

    // Inspector 에서 위치를 직접 고쳤는지 확인하고, 고쳤으면
    // 그 위치에 맞게 거리/각도를 거꾸로 계산한다.
    bool SyncFromTransformIfEditedExternally();

    Graphics* m_graphics = nullptr;

    DirectX::XMFLOAT3 m_target{ 0.0f, 0.0f, 0.0f };
    float m_distance = 60.0f;
    float m_yaw = 45.0f;      // Y축 기준 좌우 각도(도)
    float m_pitch = 35.0f;    // 수평면에서 올려본 각도(도)

    float m_fovYDegrees = 60.0f;
    float m_nearZ = 0.5f;
    float m_farZ = 1000.0f;

    float m_orbitSpeed = 0.35f;    // 픽셀당 도
    float m_zoomSpeed = 0.12f;     // 휠 눈금당 비율
    float m_moveSpeed = 30.0f;     // 초당 월드 단위

    // Transform 과의 동기화용. 우리가 마지막으로 써 넣은 값을 기억해 두었다가
    // 다음 프레임에 값이 달라져 있으면 외부(Inspector)가 고친 것으로 본다.
    DirectX::XMFLOAT3 m_lastWrittenPosition{ 0.0f, 0.0f, 0.0f };
    bool m_hasWrittenTransform = false;

    // 궤도 회전용
    bool m_orbiting = false;
    int  m_lastMouseX = 0;
    int  m_lastMouseY = 0;

    static constexpr float kMinPitch = -85.0f;
    static constexpr float kMaxPitch = 85.0f;
    static constexpr float kMinDistance = 2.0f;
    static constexpr float kMaxDistance = 500.0f;
};
