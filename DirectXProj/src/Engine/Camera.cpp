#include "Core/stdafx.h"
#include "Engine/Camera.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Input/InputManager.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"

using namespace DirectX;

void Camera::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    WriteToTransform();
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
    // Inspector 에서 Position 을 직접 고쳤다면 그 값을 궤도 파라미터로 되돌린다.
    SyncFromTransformIfEditedExternally();

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

        // 끄는 방향으로 장면이 따라오도록 부호를 맞춘다.
        m_yaw   -= dx * m_orbitSpeed;
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

    WriteToTransform();
    ApplyToGraphics();
}

// -------------------------------------------------------------
// 계산한 눈 위치와 바라보는 방향을 Transform 에 기록한다.
//  Unity 처럼 "카메라의 위치 = GameObject 의 위치" 가 되어
//  Hierarchy / Inspector 에서 실제 값이 보인다.
// -------------------------------------------------------------
void Camera::WriteToTransform()
{
    Transform* transform = GetTransform();
    if (!transform)
        return;

    const XMFLOAT3 eye = GetEyePosition();
    transform->SetLocalPosition(eye);

    // 타깃을 바라보는 회전. pitch 는 아래를 볼 때 +가 되도록 부호를 뒤집는다.
    transform->SetLocalRotationEuler(m_pitch, m_yaw + 180.0f, 0.0f);

    m_lastWrittenPosition = eye;
    m_hasWrittenTransform = true;
}

// -------------------------------------------------------------
// 외부에서 Transform 을 고쳤으면 궤도 파라미터를 거꾸로 계산한다.
//  타깃은 그대로 두고, 새 위치까지의 거리와 각도를 구한다.
// -------------------------------------------------------------
bool Camera::SyncFromTransformIfEditedExternally()
{
    Transform* transform = GetTransform();
    if (!transform || !m_hasWrittenTransform)
        return false;

    const XMFLOAT3& current = transform->GetLocalPosition();

    const float dx = current.x - m_lastWrittenPosition.x;
    const float dy = current.y - m_lastWrittenPosition.y;
    const float dz = current.z - m_lastWrittenPosition.z;

    if (dx * dx + dy * dy + dz * dz < 1.0e-6f)
        return false;    // 우리가 쓴 값 그대로다

    // 타깃 → 새 눈 위치 벡터를 구면 좌표로 되돌린다.
    const float ox = current.x - m_target.x;
    const float oy = current.y - m_target.y;
    const float oz = current.z - m_target.z;

    const float distance = std::sqrt(ox * ox + oy * oy + oz * oz);
    if (distance < 1.0e-4f)
        return false;    // 타깃과 같은 자리면 각도를 정할 수 없다

    m_distance = (std::max)(kMinDistance, (std::min)(kMaxDistance, distance));
    m_pitch = XMConvertToDegrees(std::asin((std::max)(-1.0f, (std::min)(1.0f, oy / distance))));
    m_pitch = (std::max)(kMinPitch, (std::min)(kMaxPitch, m_pitch));
    m_yaw = XMConvertToDegrees(std::atan2(ox, -oz));

    dxutil::DebugLog(L"[Camera] Inspector 편집 반영 : 거리 %.1f, yaw %.1f, pitch %.1f",
                     m_distance, m_yaw, m_pitch);
    return true;
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
