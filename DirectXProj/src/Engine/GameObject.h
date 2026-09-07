#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Engine/Transform.h"

class Scene;
class Graphics;

// =============================================================
// GameObject (과제 1, 3, 4 / S01, S05, S15, S16, S17, S18)
//
//  - 생성 시 Transform 을 반드시 하나 갖는다.
//  - Component 저장소는 unordered_map<type_index, vector<unique_ptr<Component>>>
//    → GetComponent<T>() 가 평균 O(1) 이고, 같은 타입을 여러 개 가질 수 있다. (과제 4)
//  - AddComponent / RemoveComponent 는 순회 중 컨테이너를 건드리지 않도록
//    큐에 적어 두었다가 ProcessPendingChanges 에서 한 번에 반영한다. (과제 3)
// =============================================================
class GameObject
{
public:
    using ComponentList = std::vector<std::unique_ptr<Component>>;
    using ComponentMap  = std::unordered_map<std::type_index, ComponentList>;

    explicit GameObject(const std::string& name = "GameObject");
    GameObject(uint64_t id, const std::string& name);      // JSON 로드용 : ID 를 복원한다
    ~GameObject();

    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    // ---- 식별 ----
    uint64_t GetId() const { return m_id; }
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    bool IsActive() const { return m_active; }
    void SetActive(bool active) { m_active = active; }

    bool IsPendingDestroy() const { return m_pendingDestroy; }
    void Destroy();                                        // 즉시 해제하지 않고 표시만 한다

    Scene* GetScene() const { return m_scene; }
    void   SetScene(Scene* scene) { m_scene = scene; }

    // ---- 계층 구조 (과제 3) ----
    GameObject* GetParent() const { return m_parent; }
    const std::vector<GameObject*>& GetChildren() const { return m_children; }

    // worldPositionStays == true : 재부모화 후에도 화면상의 위치/회전/크기를 유지한다.
    void SetParent(GameObject* parent, bool worldPositionStays = true);

    Transform* GetTransform() const { return m_transform; }

    // ---- Component ----
    template <typename T, typename... Args>
    T* AddComponent(Args&&... args);

    template <typename T>
    T* GetComponent() const;

    template <typename T>
    std::vector<T*> GetComponents() const;

    void RemoveComponent(Component* component);

    // JSON 로드 전용 : 큐를 거치지 않고 즉시 등록한다(아직 순회 중이 아니다).
    Component* AttachComponentImmediate(std::unique_ptr<Component> component);

    const ComponentMap& GetComponentMap() const { return m_components; }

    // ---- 생명주기 ----
    void Initialize(Graphics* graphics);
    void Update();
    void Render();
    void ProcessPendingChanges(Graphics* graphics);
    void DestroyAllComponents();

    // ---- 직렬화 (과제 4) ----
    void ToJson(json::Value& out) const;

private:
    void AddChildInternal(GameObject* child);
    void RemoveChildInternal(GameObject* child);
    bool IsAncestorOf(const GameObject* other) const;

    Component* FindComponentInPending(std::type_index type) const;

    uint64_t    m_id = 0;
    std::string m_name;
    bool        m_active = true;
    bool        m_pendingDestroy = false;

    Scene*      m_scene = nullptr;
    GameObject* m_parent = nullptr;
    std::vector<GameObject*> m_children;

    ComponentMap m_components;
    Transform*   m_transform = nullptr;

    // 지연 변경 큐 (S16)
    std::vector<std::pair<std::type_index, std::unique_ptr<Component>>> m_pendingAdd;
    std::vector<Component*> m_pendingRemove;
};

// -------------------------------------------------------------
// 템플릿 구현
// -------------------------------------------------------------
template <typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args)
{
    static_assert(std::is_base_of<Component, T>::value, "T 는 Component 를 상속해야 한다.");

    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = component.get();
    raw->SetOwner(this);

    // 순회 중일 수 있으므로 바로 넣지 않고 큐에 적어 둔다.
    m_pendingAdd.emplace_back(std::type_index(typeid(T)), std::move(component));
    return raw;
}

template <typename T>
T* GameObject::GetComponent() const
{
    static_assert(std::is_base_of<Component, T>::value, "T 는 Component 를 상속해야 한다.");

    const std::type_index type(typeid(T));

    auto it = m_components.find(type);      // 평균 O(1) (S18)
    if (it != m_components.end())
    {
        for (const auto& component : it->second)
        {
            if (component && !component->IsPendingDestroy())
                return static_cast<T*>(component.get());
        }
    }

    // 이번 프레임에 추가 예약된 것도 찾을 수 있어야 한다.
    return static_cast<T*>(FindComponentInPending(type));
}

template <typename T>
std::vector<T*> GameObject::GetComponents() const
{
    static_assert(std::is_base_of<Component, T>::value, "T 는 Component 를 상속해야 한다.");

    std::vector<T*> result;
    const std::type_index type(typeid(T));

    auto it = m_components.find(type);
    if (it != m_components.end())
    {
        result.reserve(it->second.size());
        for (const auto& component : it->second)
        {
            if (component && !component->IsPendingDestroy())
                result.push_back(static_cast<T*>(component.get()));
        }
    }

    for (const auto& pending : m_pendingAdd)
    {
        if (pending.first == type && pending.second && !pending.second->IsPendingDestroy())
            result.push_back(static_cast<T*>(pending.second.get()));
    }

    return result;
}
