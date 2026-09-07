#include "Core/stdafx.h"
#include "Engine/Transform.h"
#include "Engine/GameObject.h"

using namespace DirectX;

Transform::Transform()
    : m_localPosition(0.0f, 0.0f, 0.0f)
    , m_localRotation(0.0f, 0.0f, 0.0f, 1.0f)   // identity quaternion
    , m_localScale(1.0f, 1.0f, 1.0f)
{
    XMStoreFloat4x4(&m_worldMatrix, XMMatrixIdentity());
}

Transform* Transform::GetParentTransform() const
{
    GameObject* owner = GetOwner();
    if (!owner)
        return nullptr;

    GameObject* parent = owner->GetParent();
    return parent ? parent->GetTransform() : nullptr;
}

// -------------------------------------------------------------
// Dirty 전파 : 자신이 더러워지면 자식들의 월드 행렬도 의미가 없어진다. (S15, S16)
// -------------------------------------------------------------
void Transform::MarkDirty()
{
    if (m_dirty)
        return;    // 이미 dirty 라면 자식도 이미 dirty 다(불변 조건).

    m_dirty = true;

    GameObject* owner = GetOwner();
    if (!owner)
        return;

    for (GameObject* child : owner->GetChildren())
    {
        if (child && child->GetTransform())
            child->GetTransform()->MarkDirty();
    }
}

void Transform::SetLocalPosition(const XMFLOAT3& position)
{
    m_localPosition = position;
    MarkDirty();
}

void Transform::SetLocalPosition(float x, float y, float z)
{
    SetLocalPosition(XMFLOAT3(x, y, z));
}

void Transform::SetLocalRotation(const XMFLOAT4& quaternion)
{
    m_localRotation = quaternion;
    MarkDirty();
}

void Transform::SetLocalScale(const XMFLOAT3& scale)
{
    m_localScale = scale;
    MarkDirty();
}

void Transform::SetLocalScale(float x, float y, float z)
{
    SetLocalScale(XMFLOAT3(x, y, z));
}

void Transform::SetLocalRotationEuler(float pitchDeg, float yawDeg, float rollDeg)
{
    const XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(pitchDeg),
        XMConvertToRadians(yawDeg),
        XMConvertToRadians(rollDeg));

    XMStoreFloat4(&m_localRotation, quaternion);
    MarkDirty();
}

XMFLOAT3 Transform::GetLocalRotationEuler() const
{
    // 쿼터니언 → 회전 행렬 → 오일러 각(도). 표시/편집용 근사값이다.
    const XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&m_localRotation));

    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, rotation);

    XMFLOAT3 euler;
    const float sinPitch = -m._32;
    if (sinPitch > 0.9999f || sinPitch < -0.9999f)
    {
        euler.x = std::asin(sinPitch < -1.0f ? -1.0f : (sinPitch > 1.0f ? 1.0f : sinPitch));
        euler.y = std::atan2(-m._13, m._11);
        euler.z = 0.0f;
    }
    else
    {
        euler.x = std::asin(sinPitch);
        euler.y = std::atan2(m._31, m._33);
        euler.z = std::atan2(m._12, m._22);
    }

    euler.x = XMConvertToDegrees(euler.x);
    euler.y = XMConvertToDegrees(euler.y);
    euler.z = XMConvertToDegrees(euler.z);
    return euler;
}

void Transform::Translate(float x, float y, float z)
{
    m_localPosition.x += x;
    m_localPosition.y += y;
    m_localPosition.z += z;
    MarkDirty();
}

void Transform::RotateZ(float degrees)
{
    const XMVECTOR delta = XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, XMConvertToRadians(degrees));
    const XMVECTOR result = XMQuaternionNormalize(XMQuaternionMultiply(XMLoadFloat4(&m_localRotation), delta));

    XMStoreFloat4(&m_localRotation, result);
    MarkDirty();
}

// -------------------------------------------------------------
// 로컬 행렬 = S * R * T (행 벡터 규약, S13)
// -------------------------------------------------------------
XMMATRIX Transform::GetLocalMatrix() const
{
    const XMMATRIX scale       = XMMatrixScaling(m_localScale.x, m_localScale.y, m_localScale.z);
    const XMMATRIX rotation    = XMMatrixRotationQuaternion(XMLoadFloat4(&m_localRotation));
    const XMMATRIX translation = XMMatrixTranslation(m_localPosition.x, m_localPosition.y, m_localPosition.z);

    return scale * rotation * translation;
}

XMMATRIX Transform::GetWorldMatrix()
{
    if (!m_dirty)
        return XMLoadFloat4x4(&m_worldMatrix);

    XMMATRIX world = GetLocalMatrix();

    if (Transform* parent = GetParentTransform())
        world = world * parent->GetWorldMatrix();   // 부모가 dirty 면 여기서 연쇄적으로 갱신된다.

    XMStoreFloat4x4(&m_worldMatrix, world);
    m_dirty = false;
    return world;
}

XMFLOAT3 Transform::GetWorldPosition()
{
    const XMMATRIX world = GetWorldMatrix();

    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, world);
    return XMFLOAT3(m._41, m._42, m._43);
}

// -------------------------------------------------------------
// 월드 위치만 지정한다(회전·스케일 유지).
//  부모가 있으면 부모 월드의 역행렬로 점을 되돌려 로컬 위치를 구한다.
//  SetWorldMatrix 와 달리 분해(decompose)를 하지 않으므로 오차가 쌓이지 않는다.
// -------------------------------------------------------------
void Transform::SetWorldPosition(const XMFLOAT3& worldPosition)
{
    XMVECTOR target = XMLoadFloat3(&worldPosition);

    if (Transform* parent = GetParentTransform())
    {
        const XMMATRIX parentWorld = parent->GetWorldMatrix();

        XMVECTOR determinant = XMMatrixDeterminant(parentWorld);
        if (XMVectorGetX(XMVectorAbs(determinant)) < 1.0e-12f)
        {
            dxutil::DebugLog(L"[Transform] 부모 월드 행렬의 역행렬이 없어 SetWorldPosition 을 건너뛴다.");
            return;
        }

        const XMMATRIX inverseParent = XMMatrixInverse(&determinant, parentWorld);
        target = XMVector3TransformCoord(target, inverseParent);
    }

    XMFLOAT3 local;
    XMStoreFloat3(&local, target);
    SetLocalPosition(local);   // 내부에서 MarkDirty 가 호출된다
}

// -------------------------------------------------------------
// 월드 행렬을 직접 지정한다. 부모가 있으면 부모 월드의 역행렬을 곱해
// 로컬 행렬을 구한 뒤 S / R / T 로 분해한다. (과제 3 재부모화)
// -------------------------------------------------------------
void Transform::SetWorldMatrix(FXMMATRIX world)
{
    XMMATRIX local = world;

    if (Transform* parent = GetParentTransform())
    {
        XMVECTOR determinant = XMMatrixDeterminant(parent->GetWorldMatrix());
        const XMMATRIX inverseParent = XMMatrixInverse(&determinant, parent->GetWorldMatrix());
        local = world * inverseParent;
    }

    XMVECTOR scale, rotation, translation;
    if (XMMatrixDecompose(&scale, &rotation, &translation, local))
    {
        XMStoreFloat3(&m_localScale, scale);
        XMStoreFloat4(&m_localRotation, rotation);
        XMStoreFloat3(&m_localPosition, translation);
    }
    else
    {
        // 분해 실패(스케일 0 등) : 위치만이라도 살린다.
        XMFLOAT4X4 m;
        XMStoreFloat4x4(&m, local);
        m_localPosition = XMFLOAT3(m._41, m._42, m._43);
        dxutil::DebugLog(L"[Transform] XMMatrixDecompose 실패 : 위치만 복원했다.");
    }

    MarkDirty();
}

// -------------------------------------------------------------
// 직렬화 (과제 4)
// -------------------------------------------------------------
void Transform::ToJson(json::Value& out) const
{
    Component::ToJson(out);

    json::Value position = json::Value::MakeArray();
    position.Push(json::Value(m_localPosition.x));
    position.Push(json::Value(m_localPosition.y));
    position.Push(json::Value(m_localPosition.z));

    json::Value rotation = json::Value::MakeArray();
    rotation.Push(json::Value(m_localRotation.x));
    rotation.Push(json::Value(m_localRotation.y));
    rotation.Push(json::Value(m_localRotation.z));
    rotation.Push(json::Value(m_localRotation.w));

    json::Value scale = json::Value::MakeArray();
    scale.Push(json::Value(m_localScale.x));
    scale.Push(json::Value(m_localScale.y));
    scale.Push(json::Value(m_localScale.z));

    out["position"] = std::move(position);
    out["rotation"] = std::move(rotation);
    out["scale"]    = std::move(scale);
}

void Transform::FromJson(const json::Value& in)
{
    Component::FromJson(in);

    if (const json::Value* position = in.Find("position"))
    {
        m_localPosition.x = position->At(0).AsFloat();
        m_localPosition.y = position->At(1).AsFloat();
        m_localPosition.z = position->At(2).AsFloat();
    }

    if (const json::Value* rotation = in.Find("rotation"))
    {
        m_localRotation.x = rotation->At(0).AsFloat();
        m_localRotation.y = rotation->At(1).AsFloat();
        m_localRotation.z = rotation->At(2).AsFloat();
        m_localRotation.w = rotation->At(3).AsFloat(1.0f);
    }

    if (const json::Value* scale = in.Find("scale"))
    {
        m_localScale.x = scale->At(0).AsFloat(1.0f);
        m_localScale.y = scale->At(1).AsFloat(1.0f);
        m_localScale.z = scale->At(2).AsFloat(1.0f);
    }

    MarkDirty();
}
