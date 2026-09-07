#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"

// =============================================================
// Transform (과제 3 / S13, S14, S16)
//  - local position / rotation(quaternion) / scale 을 보관한다.
//  - 월드 행렬 = localScale * localRotation * localTranslation * 부모월드
//  - Dirty Flag : 값이 바뀐 경우에만 재계산하고, 자식에게도 전파한다.
//
//  GameObject 는 생성 시 Transform 을 반드시 하나 갖는다.
// =============================================================
class Transform : public Component
{
public:
    Transform();
    ~Transform() override = default;

    const char* GetTypeName() const override { return "Transform"; }

    // ---- 로컬 값 ----
    const DirectX::XMFLOAT3& GetLocalPosition() const { return m_localPosition; }
    const DirectX::XMFLOAT4& GetLocalRotation() const { return m_localRotation; }
    const DirectX::XMFLOAT3& GetLocalScale()    const { return m_localScale; }

    void SetLocalPosition(const DirectX::XMFLOAT3& position);
    void SetLocalPosition(float x, float y, float z);
    void SetLocalRotation(const DirectX::XMFLOAT4& quaternion);
    void SetLocalScale(const DirectX::XMFLOAT3& scale);
    void SetLocalScale(float x, float y, float z);

    // 오일러 각(도 단위)은 입력 편의를 위한 보조 API 다. 내부 표현은 쿼터니언이다. (S14)
    void SetLocalRotationEuler(float pitchDeg, float yawDeg, float rollDeg);
    DirectX::XMFLOAT3 GetLocalRotationEuler() const;

    void Translate(float x, float y, float z);
    void RotateZ(float degrees);

    // ---- 월드 값 ----
    DirectX::XMMATRIX GetWorldMatrix();
    DirectX::XMMATRIX GetLocalMatrix() const;
    DirectX::XMFLOAT3 GetWorldPosition();

    // 월드 기준 위치를 지정한다. 부모가 있으면 부모 월드의 역행렬로 되돌려
    // 로컬 위치로 바꾼다. 회전과 스케일은 건드리지 않는다. (드래그 이동에 사용)
    void SetWorldPosition(const DirectX::XMFLOAT3& worldPosition);

    // 재부모화 등에서 월드 모양을 유지할 때 사용한다.
    void SetWorldMatrix(DirectX::FXMMATRIX world);

    // ---- Dirty Flag ----
    void MarkDirty();                 // 자신과 모든 자식을 dirty 로 만든다
    bool IsDirty() const { return m_dirty; }

    // ---- 직렬화 ----
    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

private:
    Transform* GetParentTransform() const;

    DirectX::XMFLOAT3 m_localPosition;
    DirectX::XMFLOAT4 m_localRotation;   // quaternion (x, y, z, w)
    DirectX::XMFLOAT3 m_localScale;

    DirectX::XMFLOAT4X4 m_worldMatrix;
    bool m_dirty = true;
};
