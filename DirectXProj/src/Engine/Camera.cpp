#include "Core/stdafx.h"
#include "Engine/Camera.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Input/InputManager.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Engine/Scene.h"
#include "Engine/SceneManager.h"
#include "Engine/GroundProvider.h"

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
            // 걷기와 비행은 속도를 따로 기억한다. 비행 속도로 걸으면 너무 빠르다.
            float& speed = m_walking ? m_walkSpeed : m_moveSpeed;
            speed *= std::pow(m_wheelStep, notches);
            speed = (std::max)(kMinMoveSpeed, (std::min)(kMaxMoveSpeed, speed));
            dxutil::DebugLog(L"[Camera] 이동 속도 %.1f", speed);
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

        if (m_walking)
        {
            // 걷기 : 보는 방향을 수평으로 눕혀 쓴다. 하늘을 보고 W 를 눌러도 떠오르지 않는다.
            float flatX = forward.x;
            float flatZ = forward.z;
            const float flatLength = std::sqrt(flatX * flatX + flatZ * flatZ);
            if (flatLength > 1.0e-4f)
            {
                flatX /= flatLength;
                flatZ /= flatLength;
            }

            const float step = m_walkSpeed * deltaTime;
            m_position.x += (flatX * moveForward + right.x * moveRight) * step;
            m_position.z += (flatZ * moveForward + right.z * moveRight) * step;

            // 땅에 서 있을 때만 뛸 수 있다. 공중에서 또 누르면 무시한다.
            if (input.GetKeyDown(VK_SPACE) && m_grounded)
            {
                m_verticalVelocity = m_jumpSpeed;
                m_grounded = false;
            }
        }
        else if (moveForward != 0.0f || moveRight != 0.0f || moveUp != 0.0f)
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
        m_verticalVelocity = 0.0f;
    }

    // ---- G : 비행 ↔ 걷기 ----
    if (input.GetKeyDown('G'))
        SetWalking(!m_walking);

    // 이동이 끝난 위치를 땅과 맞춘다.
    ApplyGround(deltaTime);

    WriteToTransform();
    ApplyToGraphics();
}

void Camera::SetWalking(bool walking)
{
    m_walking = walking;
    m_verticalVelocity = 0.0f;
    m_grounded = false;
    dxutil::DebugLog(L"[Camera] %s", walking ? L"걷기 모드" : L"비행 모드");
}

// -------------------------------------------------------------
// 지면 처리 (S66)
//  비행 : 땅 높이 + 최소 간격 아래로는 내려가지 못한다 (땅속으로 파고들지 않게)
//  걷기 : 매 프레임 중력을 받아 떨어지고, 땅에 닿으면 눈높이에서 멈춘다
//
//  중력은 오일러 적분으로 처리한다.
//      속도 += 가속도 * dt
//      위치 += 속도   * dt
// -------------------------------------------------------------
void Camera::ApplyGround(float deltaTime)
{
    float ground = 0.0f;
    m_hasGround = QueryGround(m_position.x, m_position.z, ground);

    if (!m_hasGround)
    {
        // 지형 밖(또는 지형이 없는 씬)은 막을 것이 없다. 걷기 중이면 그 자리에 떠 있는다.
        m_grounded = false;
        m_verticalVelocity = 0.0f;
        return;
    }

    if (!m_walking)
    {
        const float lowest = ground + m_minClearance;
        if (m_position.y < lowest)
            m_position.y = lowest;
        return;
    }

    // 씬을 불러온 직후처럼 한 프레임이 길면 한 번에 땅을 뚫고 떨어지므로 잘라 둔다.
    const float dt = (std::min)(deltaTime, 0.1f);
    const float feet = ground + m_eyeHeight;

    // 내리막 : 땅이 중력보다 빨리 내려가면 계단을 뛰어내리듯 통통 튄다.
    //  방금까지 서 있었고 낙차가 작으면 떨어뜨리지 않고 땅에 붙인다. 오르막도 여기서 올라선다.
    if (m_grounded && m_verticalVelocity <= 0.0f && m_position.y - feet <= m_snapDistance)
    {
        m_position.y = feet;
        return;
    }

    m_verticalVelocity -= m_gravity * dt;
    m_position.y += m_verticalVelocity * dt;

    if (m_position.y <= feet)
    {
        m_position.y = feet;          // 착지
        m_verticalVelocity = 0.0f;
        m_grounded = true;
    }
    else
    {
        m_grounded = false;
    }
}

// -------------------------------------------------------------
// 씬에서 땅 높이를 알려 줄 수 있는 컴포넌트를 찾는다.
//  카메라는 지형의 종류를 모른다. IGroundProvider 를 구현했는지만 본다.
//  씬을 다시 불러와도(F9) 따로 연결할 필요가 없다. 여러 개가 겹치면 가장 높은 땅을 쓴다.
// -------------------------------------------------------------
bool Camera::QueryGround(float x, float z, float& outHeight) const
{
    const GameObject* owner = GetOwner();
    const Scene* scene = (owner && owner->GetScene()) ? owner->GetScene()
                                                      : SceneManager::Get().GetActiveScene();
    if (!scene)
        return false;

    bool found = false;

    for (const auto& object : scene->GetGameObjects())
    {
        if (!object || object->IsPendingDestroy() || !object->IsActive())
            continue;

        for (const auto& entry : object->GetComponentMap())
        {
            for (const auto& component : entry.second)
            {
                if (!component || component->IsPendingDestroy() || !component->IsEnabled())
                    continue;

                const IGroundProvider* provider = dynamic_cast<const IGroundProvider*>(component.get());
                float height = 0.0f;

                if (provider && provider->TryGetGroundHeight(x, z, height))
                {
                    outHeight = found ? (std::max)(outHeight, height) : height;
                    found = true;
                }
            }
        }
    }

    return found;
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
    out["walking"]   = json::Value(m_walking);
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
    if (const json::Value* value = in.Find("walking"))   m_walking     = value->AsBool(m_walking);

    m_pitch = (std::max)(-kMaxPitch, (std::min)(kMaxPitch, m_pitch));
}
