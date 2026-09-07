#include "Core/stdafx.h"
#include "Engine/Camera.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Input/InputManager.h"

using namespace DirectX;

void Camera::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    ApplyToGraphics();
}

XMFLOAT3 Camera::GetEyePosition() const
{
    // 구면 좌표 → 직교 좌표.
    //  pitch 를 올리면 위로, yaw 를 돌리면 수평으로 공전한다.
    const float yaw = XMConvertToRadians(m_yaw);
    const float pitch = XMConvertToRadians(m_pitch);

    const float horizontal = m_distance * std::cos(pitch);

    return XMFLOAT3(
        m_target.x + horizontal * std::sin(yaw),
        m_target.y + m_distance * std::sin(pitch),
        m_target.z - horizontal * std::cos(yaw));
}

void Camera::Update()
{
    const InputManager& input = InputManager::Get();
    const float deltaTime = TimeManager::Get().GetDeltaTime();

    // ---- 궤도 회전 : 가운데 버튼 드래그 ----
    if (input.GetMouseButtonDown(InputManager::Middle))
    {
        m_orbiting = true;
        m_lastMouseX = input.GetMouseX();
        m_lastMouseY = input.GetMouseY();
    }

    if (!input.GetMouseButton(InputManager::Middle))
        m_orbiting = false;

    if (m_orbiting)
    {
        const int dx = input.GetMouseX() - m_lastMouseX;
        const int dy = input.GetMouseY() - m_lastMouseY;

        m_yaw   += dx * m_orbitSpeed;
        m_pitch += dy * m_orbitSpeed;

        // 위/아래를 넘어가면 화면이 뒤집힌다. 각도를 제한한다.
        m_pitch = (std::max)(kMinPitch, (std::min)(kMaxPitch, m_pitch));

        m_lastMouseX = input.GetMouseX();
        m_lastMouseY = input.GetMouseY();
    }

    // ---- 줌 : 휠 ----
    if (const int wheel = input.GetMouseWheelDelta(); wheel != 0)
    {
        const float notches = static_cast<float>(wheel) / WHEEL_DELTA;
        m_distance *= std::pow(1.0f - m_zoomSpeed, notches);
        m_distance = (std::max)(kMinDistance, (std::min)(kMaxDistance, m_distance));
    }

    // ---- 타깃 이동 : WASD / QE ----
    //  카메라가 보는 방향 기준으로 움직여야 직관적이다.
    const float yaw = XMConvertToRadians(m_yaw);
    const XMFLOAT3 forward(std::sin(yaw), 0.0f, -std::cos(yaw));   // 수평 전방
    const XMFLOAT3 right(std::cos(yaw), 0.0f, std::sin(yaw));      // 수평 우측

    float moveForward = 0.0f;
    float moveRight = 0.0f;
    float moveUp = 0.0f;

    if (input.GetKey('W')) moveForward -= 1.0f;   // 화면 위쪽(멀어지는 쪽)
    if (input.GetKey('S')) moveForward += 1.0f;
    if (input.GetKey('A')) moveRight   -= 1.0f;
    if (input.GetKey('D')) moveRight   += 1.0f;
    if (input.GetKey('E')) moveUp      += 1.0f;
    if (input.GetKey('Q')) moveUp      -= 1.0f;

    if (moveForward != 0.0f || moveRight != 0.0f || moveUp != 0.0f)
    {
        // 멀리서 볼수록 빠르게 움직여야 답답하지 않다.
        const float speed = m_moveSpeed * deltaTime * (m_distance / 60.0f);

        m_target.x += (forward.x * moveForward + right.x * moveRight) * speed;
        m_target.z += (forward.z * moveForward + right.z * moveRight) * speed;
        m_target.y += moveUp * speed;
    }

    // ---- 리셋 ----
    if (input.GetKeyDown('F'))
    {
        m_target = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_distance = 60.0f;
        m_yaw = 45.0f;
        m_pitch = 35.0f;
    }

    ApplyToGraphics();
}

void Camera::ApplyToGraphics()
{
    if (!m_graphics)
        return;

    const XMFLOAT3 eye = GetEyePosition();

    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(eye.x, eye.y, eye.z, 1.0f),
        XMVectorSet(m_target.x, m_target.y, m_target.z, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    // 원근 투영 : 화면 비율이 바뀌면 아스펙트도 따라가야 찌그러지지 않는다.
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(m_fovYDegrees),
        m_graphics->GetAspectRatio(),
        m_nearZ,
        m_farZ);

    m_graphics->SetCamera3D(view, projection, eye);
}

void Camera::ToJson(json::Value& out) const
{
    Component::ToJson(out);

    json::Value target = json::Value::MakeArray();
    target.Push(json::Value(m_target.x));
    target.Push(json::Value(m_target.y));
    target.Push(json::Value(m_target.z));

    out["target"]   = std::move(target);
    out["distance"] = json::Value(m_distance);
    out["yaw"]      = json::Value(m_yaw);
    out["pitch"]    = json::Value(m_pitch);
    out["fovY"]     = json::Value(m_fovYDegrees);
}

void Camera::FromJson(const json::Value& in)
{
    Component::FromJson(in);

    if (const json::Value* target = in.Find("target"))
    {
        m_target.x = target->At(0).AsFloat();
        m_target.y = target->At(1).AsFloat();
        m_target.z = target->At(2).AsFloat();
    }

    if (const json::Value* value = in.Find("distance")) m_distance    = value->AsFloat(m_distance);
    if (const json::Value* value = in.Find("yaw"))      m_yaw         = value->AsFloat(m_yaw);
    if (const json::Value* value = in.Find("pitch"))    m_pitch       = value->AsFloat(m_pitch);
    if (const json::Value* value = in.Find("fovY"))     m_fovYDegrees = value->AsFloat(m_fovYDegrees);
}
