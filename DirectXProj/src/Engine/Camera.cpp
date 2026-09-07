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

void Camera::SetAngles(float yawDeg, float pitchDeg)
{
    m_yaw = yawDeg;
    m_pitch = (std::max)(-kMaxPitch, (std::min)(kMaxPitch, pitchDeg));
}

// -------------------------------------------------------------
// 보는 방향과 오른쪽 방향
//  yaw = 0 이면 +Z 를 본다. pitch 가 +면 위를 본다.
// -------------------------------------------------------------
XMFLOAT3 Camera::GetForward() const
{
    const float yaw = XMConvertToRadians(m_yaw);
    const float pitch = XMConvertToRadians(m_pitch);
    const float cosPitch = std::cos(pitch);

    return XMFLOAT3(cosPitch * std::sin(yaw),
                    std::sin(pitch),
                    cosPitch * std::cos(yaw));
}

XMFLOAT3 Camera::GetRight() const
{
    // 전방을 Y축 기준으로 +90도 돌린 수평 벡터. pitch 와 무관하다.
    const float yaw = XMConvertToRadians(m_yaw);
    return XMFLOAT3(std::cos(yaw), 0.0f, -std::sin(yaw));
}

void Camera::LookAt(const XMFLOAT3& target)
{
    const float dx = target.x - m_position.x;
    const float dy = target.y - m_position.y;
    const float dz = target.z - m_position.z;

    const float horizontal = std::sqrt(dx * dx + dz * dz);

    m_yaw = XMConvertToDegrees(std::atan2(dx, dz));
    m_pitch = XMConvertToDegrees(std::atan2(dy, horizontal));
    m_pitch = (std::max)(-kMaxPitch, (std::min)(kMaxPitch, m_pitch));
}

void Camera::Update()
{
    // Inspector 에서 Transform 을 직접 고쳤다면 그 값을 먼저 받아들인다.
    SyncFromTransformIfEditedExternally();

    const InputManager& input = InputManager::Get();
    const float deltaTime = TimeManager::Get().GetDeltaTime();

    // ---- 오른쪽 버튼을 누른 동안에만 조작한다 ----
    const bool rightDown = input.GetMouseButton(InputManager::Right);

    if (input.GetMouseButtonDown(InputManager::Right))
    {
        m_looking = true;
        m_lastMouseX = input.GetMouseX();
        m_lastMouseY = input.GetMouseY();
    }

    if (!rightDown)
        m_looking = false;

    if (m_looking)
    {
        // ---- 회전 ----
        const int dx = input.GetMouseX() - m_lastMouseX;
        const int dy = input.GetMouseY() - m_lastMouseY;

        m_yaw += dx * m_lookSpeed;             // 좌우는 제한 없이 계속 돈다
        m_pitch -= dy * m_lookSpeed;           // 마우스를 내리면 아래를 본다

        // 상하만 자른다. 넘어가면 화면이 뒤집히고 ±90도에서는 뷰 행렬이 무너진다.
        m_pitch = (std::max)(-kMaxPitch, (std::min)(kMaxPitch, m_pitch));

        // yaw 가 무한히 커지지 않도록 -180~180 으로 접어 준다(값만 정리, 동작은 같다).
        if (m_yaw > 180.0f)  m_yaw -= 360.0f;
        if (m_yaw < -180.0f) m_yaw += 360.0f;

        m_lastMouseX = input.GetMouseX();
        m_lastMouseY = input.GetMouseY();

        // ---- 휠로 이동 속도 조절 ----
        if (const int wheel = input.GetMouseWheelDelta(); wheel != 0)
        {
            const float notches = static_cast<float>(wheel) / WHEEL_DELTA;
            m_moveSpeed *= std::pow(m_wheelStep, notches);
            m_moveSpeed = (std::max)(kMinMoveSpeed, (std::min)(kMaxMoveSpeed, m_moveSpeed));
            dxutil::DebugLog(L"[Camera] 이동 속도 %.1f", m_moveSpeed);
        }

        // ---- 이동 ----
        const XMFLOAT3 forward = GetForward();
        const XMFLOAT3 right = GetRight();

        float moveForward = 0.0f;
        float moveRight = 0.0f;
        float moveUp = 0.0f;

        if (input.GetKey('W')) moveForward += 1.0f;
        if (input.GetKey('S')) moveForward -= 1.0f;
        if (input.GetKey('D')) moveRight   += 1.0f;
        if (input.GetKey('A')) moveRight   -= 1.0f;
        if (input.GetKey('E')) moveUp      += 1.0f;
        if (input.GetKey('Q')) moveUp      -= 1.0f;

        if (moveForward != 0.0f || moveRight != 0.0f || moveUp != 0.0f)
        {
            const float step = m_moveSpeed * deltaTime;

            m_position.x += (forward.x * moveForward + right.x * moveRight) * step;
            m_position.y += (forward.y * moveForward + moveUp) * step;
            m_position.z += (forward.z * moveForward + right.z * moveRight) * step;
        }
    }

    // ---- 리셋 (버튼과 무관) ----
    if (input.GetKeyDown('F'))
    {
        m_position = XMFLOAT3(0.0f, 60.0f, -90.0f);
        LookAt(XMFLOAT3(0.0f, 0.0f, 0.0f));
    }

    WriteToTransform();
    ApplyToGraphics();
}

// -------------------------------------------------------------
// 카메라의 위치·회전을 소유 GameObject 의 Transform 에 기록한다.
//  Unity 처럼 "카메라의 위치 = GameObject 의 위치" 가 되어
//  Hierarchy / Inspector 에서 실제 값이 보인다.
// -------------------------------------------------------------
void Camera::WriteToTransform()
{
    Transform* transform = GetTransform();
    if (!transform)
        return;

    transform->SetLocalPosition(m_position);
    transform->SetLocalRotationEuler(-m_pitch, m_yaw, 0.0f);

    m_lastWrittenPosition = m_position;
    m_lastWrittenEuler = XMFLOAT3(-m_pitch, m_yaw, 0.0f);
    m_hasWrittenTransform = true;
}

// -------------------------------------------------------------
// Inspector 에서 값을 고쳤으면 그것을 카메라 상태로 받아들인다.
//  우리가 마지막으로 써 넣은 값과 달라졌는지로 판단한다.
// -------------------------------------------------------------
bool Camera::SyncFromTransformIfEditedExternally()
{
    Transform* transform = GetTransform();
    if (!transform || !m_hasWrittenTransform)
        return false;

    bool changed = false;

    const XMFLOAT3& position = transform->GetLocalPosition();
    const float dx = position.x - m_lastWrittenPosition.x;
    const float dy = position.y - m_lastWrittenPosition.y;
    const float dz = position.z - m_lastWrittenPosition.z;

    if (dx * dx + dy * dy + dz * dz > 1.0e-6f)
    {
        m_position = position;
        changed = true;
    }

    const XMFLOAT3 euler = transform->GetLocalRotationEuler();
    if (std::fabs(euler.x - m_lastWrittenEuler.x) > 0.01f ||
        std::fabs(euler.y - m_lastWrittenEuler.y) > 0.01f)
    {
        m_pitch = (std::max)(-kMaxPitch, (std::min)(kMaxPitch, -euler.x));
        m_yaw = euler.y;
        changed = true;
    }

    if (changed)
        dxutil::DebugLog(L"[Camera] Inspector 편집 반영 : yaw %.1f, pitch %.1f", m_yaw, m_pitch);

    return changed;
}

void Camera::ApplyToGraphics()
{
    if (!m_graphics)
        return;

    const XMFLOAT3 forward = GetForward();

    // LookTo 는 "어느 점" 이 아니라 "어느 방향" 을 본다. 프리룩 카메라에 맞는 형태다.
    const XMMATRIX view = XMMatrixLookToLH(
        XMVectorSet(m_position.x, m_position.y, m_position.z, 1.0f),
        XMVectorSet(forward.x, forward.y, forward.z, 0.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(m_fovYDegrees),
        m_graphics->GetAspectRatio(),
        m_nearZ,
        m_farZ);

    m_graphics->SetCamera3D(view, projection, m_position);
}

void Camera::ToJson(json::Value& out) const
{
    Component::ToJson(out);

    json::Value position = json::Value::MakeArray();
    position.Push(json::Value(m_position.x));
    position.Push(json::Value(m_position.y));
    position.Push(json::Value(m_position.z));

    out["position"]  = std::move(position);
    out["yaw"]       = json::Value(m_yaw);
    out["pitch"]     = json::Value(m_pitch);
    out["fovY"]      = json::Value(m_fovYDegrees);
    out["moveSpeed"] = json::Value(m_moveSpeed);
}

void Camera::FromJson(const json::Value& in)
{
    Component::FromJson(in);

    if (const json::Value* position = in.Find("position"))
    {
        m_position.x = position->At(0).AsFloat();
        m_position.y = position->At(1).AsFloat();
        m_position.z = position->At(2).AsFloat();
    }

    if (const json::Value* value = in.Find("yaw"))       m_yaw         = value->AsFloat(m_yaw);
    if (const json::Value* value = in.Find("pitch"))     m_pitch       = value->AsFloat(m_pitch);
    if (const json::Value* value = in.Find("fovY"))      m_fovYDegrees = value->AsFloat(m_fovYDegrees);
    if (const json::Value* value = in.Find("moveSpeed")) m_moveSpeed   = value->AsFloat(m_moveSpeed);

    m_pitch = (std::max)(-kMaxPitch, (std::min)(kMaxPitch, m_pitch));
}
