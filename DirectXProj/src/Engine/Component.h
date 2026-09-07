#pragma once
#include "Core/stdafx.h"
#include "Utils/Json.h"

class GameObject;
class Transform;
class Graphics;

// =============================================================
// Component (과제 1, 4, 5 / S05)
//  생명주기
//      Initialize(graphics)  : GPU 리소스 준비 (Graphics 초기화 이후 1회)
//      Start()               : 첫 Update 직전 1회
//      Update()              : 매 프레임
//      Render()              : 매 프레임, 모든 Update 이후
//      OnDestroy()           : 제거가 확정되는 프레임 끝에 1회
//
//  생성 시 고유 ID 를 발급받고 ObjectRegistry 에 등록된다. (과제 4)
//  ToJson / FromJson 으로 자신의 데이터를 직렬화한다.
// =============================================================
class Component
{
public:
    Component();
    virtual ~Component();

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    // ComponentFactory 의 키가 되는 이름. JSON 의 "type" 값과 같다.
    virtual const char* GetTypeName() const = 0;

    // ---- 생명주기 ----
    virtual void Initialize(Graphics* graphics) { (void)graphics; }
    virtual void Start() {}
    virtual void Update() {}
    virtual void Render() {}
    virtual void OnDestroy() {}

    // ---- 직렬화 ----
    // 기본 구현은 type / id / enabled 를 기록한다. 파생 클래스는 먼저 base 를 호출한다.
    virtual void ToJson(json::Value& out) const;
    virtual void FromJson(const json::Value& in);

    // ---- 소유/식별 ----
    GameObject* GetOwner() const { return m_owner; }
    void        SetOwner(GameObject* owner) { m_owner = owner; }

    uint64_t GetId() const { return m_id; }
    void     SetId(uint64_t id);          // 로드 시 저장된 ID 를 복원한다

    Transform* GetTransform() const;      // 소유 GameObject 의 Transform

    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

    // ---- 지연 삭제 (과제 3 / S16) ----
    bool IsPendingDestroy() const { return m_pendingDestroy; }
    void MarkPendingDestroy() { m_pendingDestroy = true; }

    bool IsStarted() const { return m_started; }
    void MarkStarted() { m_started = true; }

    bool IsInitialized() const { return m_initialized; }
    void MarkInitialized() { m_initialized = true; }

private:
    GameObject* m_owner = nullptr;
    uint64_t    m_id = 0;
    bool        m_enabled = true;
    bool        m_pendingDestroy = false;
    bool        m_started = false;
    bool        m_initialized = false;
};
