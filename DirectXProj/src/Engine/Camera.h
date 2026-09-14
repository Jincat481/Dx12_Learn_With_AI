#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"

class Graphics;

// =============================================================
// Camera (S27, S28)
//  우클릭 프리룩 카메라. Unity 씬 뷰의 비행 조작과 같은 방식이다.
//
//  카메라는 위치 하나와 각도 둘(yaw, pitch)로 정해진다.
//      forward = ( cosP·sinY,  sinP,  cosP·cosY )
//      right   = ( cosY,       0,    -sinY      )
//
//  조작 — 오른쪽 버튼을 누르고 있는 동안에만 동작한다
//    마우스 이동 : 상하좌우 회전 (좌우 무제한, 상하 ±90도 제한)
//    W / S      : 보는 방향으로 전진 / 후진
//    A / D      : 좌우 평행 이동
//    Q / E      : 월드 기준 아래 / 위
//    휠         : 이동 속도 조절
//  F : 기본 위치로 리셋 (버튼과 무관)
//  G : 비행 ↔ 걷기 (S66)
//      비행 : 땅속으로만 못 들어간다
//      걷기 : 중력을 받아 땅 위에 눈높이만큼 떠서 선다. 우클릭 + Space 로 점프
//
//  왜 상하만 제한하나 :
//   pitch 가 ±90도를 넘으면 화면이 뒤집힌다. 정확히 ±90도에서는
//   보는 방향과 up 벡터(0,1,0)가 평행해져 뷰 행렬 자체를 만들 수 없다.
//   그래서 아주 살짝 못 미치는 값으로 자른다.
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

    void SetPosition(const DirectX::XMFLOAT3& position) { m_position = position; }
    void SetAngles(float yawDeg, float pitchDeg);

    // 지금 위치에서 한 점을 바라보도록 yaw / pitch 를 맞춘다. (씬 초기 배치용)
    void LookAt(const DirectX::XMFLOAT3& target);

    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    DirectX::XMFLOAT3 GetForward() const;
    DirectX::XMFLOAT3 GetRight() const;

    // ---- 보강 : 지면 (S66) ----
    void SetWalking(bool walking);
    bool IsWalking() const { return m_walking; }
    bool HasGround() const { return m_hasGround; }

private:
    void ApplyToGraphics();
    void WriteToTransform();
    bool SyncFromTransformIfEditedExternally();

    void ApplyGround(float deltaTime);
    bool QueryGround(float x, float z, float& outHeight) const;

    Graphics* m_graphics = nullptr;

    DirectX::XMFLOAT3 m_position{ 0.0f, 45.0f, -80.0f };
    float m_yaw = 0.0f;      // Y축 기준 좌우 각도(도). 제한 없음
    float m_pitch = -20.0f;  // 위(+) 아래(-) 각도(도). ±90도로 제한

    float m_fovYDegrees = 60.0f;
    float m_nearZ = 0.5f;
    float m_farZ = 1000.0f;

    float m_lookSpeed = 0.15f;    // 픽셀당 도
    float m_moveSpeed = 40.0f;    // 초당 월드 단위
    float m_wheelStep = 1.15f;    // 휠 한 눈금당 이동 속도 배율

    // 지면 (S66)
    bool  m_walking = false;
    bool  m_hasGround = false;        // 발밑에 땅이 있는가 (지형 밖이면 false)
    bool  m_grounded = false;         // 땅에 발을 딛고 있는가
    float m_walkSpeed = 14.0f;        // 걷기 속도 (초당 월드 단위)
    float m_eyeHeight = 1.8f;         // 발에서 눈까지
    float m_minClearance = 1.0f;      // 비행 중 땅과 유지할 최소 간격
    float m_gravity = 30.0f;          // 초당 속도 변화량
    float m_jumpSpeed = 11.0f;
    float m_verticalVelocity = 0.0f;
    float m_snapDistance = 1.2f;      // 내리막에서 이만큼 차이는 떨어지지 않고 붙어 간다

    // 우클릭 룩 상태
    bool m_looking = false;
    int  m_lastMouseX = 0;
    int  m_lastMouseY = 0;

    // Transform 과의 양방향 동기화용
    DirectX::XMFLOAT3 m_lastWrittenPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_lastWrittenEuler{ 0.0f, 0.0f, 0.0f };
    bool m_hasWrittenTransform = false;

    // 정확히 90도면 up 벡터와 평행해져 뷰 행렬이 무너진다.
    static constexpr float kMaxPitch = 89.9f;
    static constexpr float kMinMoveSpeed = 2.0f;
    static constexpr float kMaxMoveSpeed = 400.0f;
};
