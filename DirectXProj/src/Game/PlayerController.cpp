#include "Core/stdafx.h"
#include "Game/PlayerController.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Core/TimeManager.h"
#include "Input/InputManager.h"

using namespace DirectX;

void PlayerController::Start()
{
    if (GetOwner())
        dxutil::DebugLog(L"[PlayerController] Start : %S", GetOwner()->GetName().c_str());
}

void PlayerController::Update()
{
    Transform* transform = GetTransform();
    if (!transform)
        return;

    const InputManager& input = InputManager::Get();
    const float deltaTime = TimeManager::Get().GetDeltaTime();

    // ---- 이동 ----
    float dx = 0.0f;
    float dy = 0.0f;

    if (input.GetKey('A') || input.GetKey(VK_LEFT))  dx -= 1.0f;
    if (input.GetKey('D') || input.GetKey(VK_RIGHT)) dx += 1.0f;
    if (input.GetKey('W') || input.GetKey(VK_UP))    dy += 1.0f;   // 월드는 +Y 가 위쪽
    if (input.GetKey('S') || input.GetKey(VK_DOWN))  dy -= 1.0f;

    if (dx != 0.0f || dy != 0.0f)
    {
        // 대각선 이동이 빨라지지 않도록 정규화한다.
        const float length = std::sqrt(dx * dx + dy * dy);
        dx /= length;
        dy /= length;

        transform->Translate(dx * m_moveSpeed * deltaTime, dy * m_moveSpeed * deltaTime, 0.0f);
    }

    // ---- 회전 ----
    if (input.GetKey('Q')) transform->RotateZ( m_rotateSpeed * deltaTime);
    if (input.GetKey('E')) transform->RotateZ(-m_rotateSpeed * deltaTime);

    // ---- 스케일 ----
    if (input.GetKey('Z') || input.GetKey('X'))
    {
        const float sign = input.GetKey('Z') ? 1.0f : -1.0f;
        XMFLOAT3 scale = transform->GetLocalScale();

        const float delta = sign * m_scaleSpeed * deltaTime;
        scale.x = (std::max)(0.1f, scale.x + delta);
        scale.y = (std::max)(0.1f, scale.y + delta);

        transform->SetLocalScale(scale);
    }

    // ---- 초기화 ----
    if (input.GetKeyDown('R'))
    {
        transform->SetLocalPosition(0.0f, 0.0f, 0.0f);
        transform->SetLocalRotation(XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        transform->SetLocalScale(1.0f, 1.0f, 1.0f);
    }
}

void PlayerController::ToJson(json::Value& out) const
{
    Component::ToJson(out);

    out["moveSpeed"]   = json::Value(m_moveSpeed);
    out["rotateSpeed"] = json::Value(m_rotateSpeed);
    out["scaleSpeed"]  = json::Value(m_scaleSpeed);
}

void PlayerController::FromJson(const json::Value& in)
{
    Component::FromJson(in);

    if (const json::Value* value = in.Find("moveSpeed"))   m_moveSpeed   = value->AsFloat(m_moveSpeed);
    if (const json::Value* value = in.Find("rotateSpeed")) m_rotateSpeed = value->AsFloat(m_rotateSpeed);
    if (const json::Value* value = in.Find("scaleSpeed"))  m_scaleSpeed  = value->AsFloat(m_scaleSpeed);
}
