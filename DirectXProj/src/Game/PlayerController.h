#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"

// =============================================================
// PlayerController (샘플 Component)
//  프레임워크 동작을 눈으로 확인하기 위한 데모용 컴포넌트다.
//   - WASD / 방향키 : 이동 (Delta Time 기반)
//   - Q / E         : Z축 회전
//   - Z / X         : 스케일
//   - R             : 로컬 값 초기화
//
//  ComponentFactory 에 등록되므로 JSON 저장/로드에도 함께 포함된다.
// =============================================================
class PlayerController : public Component
{
public:
    PlayerController() = default;
    ~PlayerController() override = default;

    const char* GetTypeName() const override { return "PlayerController"; }

    void Start() override;
    void Update() override;

    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    void SetRotateSpeed(float speed) { m_rotateSpeed = speed; }

private:
    float m_moveSpeed = 300.0f;      // 초당 픽셀
    float m_rotateSpeed = 120.0f;    // 초당 도(degree)
    float m_scaleSpeed = 1.0f;       // 초당 배율 변화
};
