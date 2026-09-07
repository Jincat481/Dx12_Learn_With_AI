#include "Core/stdafx.h"
#include "Engine/GameObject.h"
#include "Engine/Scene.h"
#include "Core/ObjectRegistry.h"
#include "Utils/IdGenerator.h"

using namespace DirectX;

GameObject::GameObject(const std::string& name)
    : m_id(IdGenerator::NextGameObjectId())
    , m_name(name)
{
    ObjectRegistry::Get().RegisterGameObject(m_id, this);

    // 모든 GameObject 는 Transform 을 하나 갖는다(즉시 등록).
    auto transform = std::make_unique<Transform>();
    m_transform = transform.get();
    AttachComponentImmediate(std::move(transform));
}

GameObject::GameObject(uint64_t id, const std::string& name)
    : m_id(id)
    , m_name(name)
{
    IdGenerator::EnsureGameObjectIdAbove(id);
    ObjectRegistry::Get().RegisterGameObject(m_id, this);

    auto transform = std::make_unique<Transform>();
    m_transform = transform.get();
    AttachComponentImmediate(std::move(transform));
}

GameObject::~GameObject()
{
    // 부모/자식 연결을 끊는다(끊지 않으면 파괴된 포인터가 남는다).
    if (m_parent)
        m_parent->RemoveChildInternal(this);

    for (GameObject* child : m_children)
    {
        if (child)
            child->m_parent = nullptr;
    }
    m_children.clear();

    ObjectRegistry::Get().UnregisterGameObject(m_id);
}

void GameObject::Destroy()
{
    if (m_pendingDestroy)
        return;

    // 즉시 메모리를 해제하지 않는다. 프레임 끝에 Scene 이 안전하게 제거한다. (과제 3)
    m_pendingDestroy = true;
    m_active = false;

    for (GameObject* child : m_children)
    {
        if (child)
            child->Destroy();
    }
}

// -------------------------------------------------------------
// Component 등록/해제
// -------------------------------------------------------------
Component* GameObject::AttachComponentImmediate(std::unique_ptr<Component> component)
{
    if (!component)
        return nullptr;

    component->SetOwner(this);

    Component* raw = component.get();
    const std::type_index type(typeid(*raw));    // 동적 타입으로 등록한다 (S17)

    m_components[type].push_back(std::move(component));

    if (!m_transform)
    {
        if (Transform* transform = dynamic_cast<Transform*>(raw))
            m_transform = transform;
    }
    return raw;
}

void GameObject::RemoveComponent(Component* component)
{
    if (!component || component == m_transform)
        return;   // Transform 은 제거할 수 없다.

    if (component->IsPendingDestroy())
        return;

    // 즉시 Update/Render 대상에서 빠지고, 실제 제거는 프레임 끝에 이뤄진다.
    component->MarkPendingDestroy();
    m_pendingRemove.push_back(component);
}

Component* GameObject::FindComponentInPending(std::type_index type) const
{
    for (const auto& pending : m_pendingAdd)
    {
        if (pending.first == type && pending.second && !pending.second->IsPendingDestroy())
            return pending.second.get();
    }
    return nullptr;
}

void GameObject::DestroyAllComponents()
{
    for (auto& bucket : m_components)
    {
        for (auto& component : bucket.second)
        {
            if (component)
                component->OnDestroy();
        }
    }
    m_components.clear();
    m_pendingAdd.clear();
    m_pendingRemove.clear();
    m_transform = nullptr;
}

// -------------------------------------------------------------
// 계층 구조 (과제 3)
// -------------------------------------------------------------
void GameObject::AddChildInternal(GameObject* child)
{
    if (!child)
        return;

    if (std::find(m_children.begin(), m_children.end(), child) == m_children.end())
        m_children.push_back(child);
}

void GameObject::RemoveChildInternal(GameObject* child)
{
    m_children.erase(std::remove(m_children.begin(), m_children.end(), child), m_children.end());
}

bool GameObject::IsAncestorOf(const GameObject* other) const
{
    const GameObject* current = other ? other->m_parent : nullptr;
    while (current)
    {
        if (current == this)
            return true;
        current = current->m_parent;
    }
    return false;
}

void GameObject::SetParent(GameObject* parent, bool worldPositionStays)
{
    if (parent == m_parent)
        return;

    if (parent == this)
    {
        dxutil::DebugLog(L"[GameObject] 자기 자신을 부모로 지정할 수 없다.");
        return;
    }

    // 순환 참조 방지 : 자기 자손을 부모로 삼을 수 없다. (S15)
    if (parent && IsAncestorOf(parent))
    {
        dxutil::DebugLog(L"[GameObject] 자손을 부모로 지정할 수 없다(순환 참조).");
        return;
    }

    // 변경 전 월드 행렬을 먼저 확보한다.
    XMMATRIX worldBefore = XMMatrixIdentity();
    if (worldPositionStays && m_transform)
        worldBefore = m_transform->GetWorldMatrix();

    // 기존 부모에서 분리 → 새 부모에 연결
    if (m_parent)
        m_parent->RemoveChildInternal(this);

    m_parent = parent;

    if (m_parent)
        m_parent->AddChildInternal(this);

    // 부모가 바뀌면 월드 행렬 캐시는 무효다.
    if (m_transform)
    {
        m_transform->MarkDirty();

        if (worldPositionStays)
        {
            // 새 부모 월드의 역행렬로 새 로컬 행렬을 구해 분해한다.
            m_transform->SetWorldMatrix(worldBefore);
        }
    }
}

// -------------------------------------------------------------
// 생명주기
// -------------------------------------------------------------
void GameObject::Initialize(Graphics* graphics)
{
    for (auto& bucket : m_components)
    {
        for (auto& component : bucket.second)
        {
            if (component && !component->IsInitialized())
            {
                component->Initialize(graphics);
                component->MarkInitialized();
            }
        }
    }
}

void GameObject::Update()
{
    if (!m_active || m_pendingDestroy)
        return;

    for (auto& bucket : m_components)
    {
        for (auto& component : bucket.second)
        {
            if (!component || component->IsPendingDestroy() || !component->IsEnabled())
                continue;

            if (!component->IsStarted())
            {
                component->Start();
                component->MarkStarted();
            }

            component->Update();
        }
    }
}

void GameObject::Render()
{
    if (!m_active || m_pendingDestroy)
        return;

    for (auto& bucket : m_components)
    {
        for (auto& component : bucket.second)
        {
            if (!component || component->IsPendingDestroy() || !component->IsEnabled())
                continue;

            component->Render();
        }
    }
}

void GameObject::ProcessPendingChanges(Graphics* graphics)
{
    // 1) 삭제 예약분을 실제로 제거한다.
    for (Component* target : m_pendingRemove)
    {
        if (!target)
            continue;

        auto it = m_components.find(std::type_index(typeid(*target)));
        if (it == m_components.end())
            continue;

        ComponentList& list = it->second;
        for (auto listIt = list.begin(); listIt != list.end(); ++listIt)
        {
            if (listIt->get() != target)
                continue;

            target->OnDestroy();
            list.erase(listIt);      // unique_ptr 소멸 → ObjectRegistry 등록 해제
            break;
        }

        if (list.empty())
            m_components.erase(it);
    }
    m_pendingRemove.clear();

    // 2) 추가 예약분을 반영한다.
    if (!m_pendingAdd.empty())
    {
        // 반영 중에 또 추가될 수 있으므로 큐를 먼저 옮겨 둔다.
        auto pending = std::move(m_pendingAdd);
        m_pendingAdd.clear();

        for (auto& entry : pending)
        {
            if (!entry.second)
                continue;

            Component* raw = entry.second.get();
            m_components[entry.first].push_back(std::move(entry.second));

            // 이미 Graphics 가 준비된 뒤 추가된 Component 도 초기화해 준다.
            if (graphics && !raw->IsInitialized())
            {
                raw->Initialize(graphics);
                raw->MarkInitialized();
            }
        }
    }
}

// -------------------------------------------------------------
// 직렬화 (과제 4)
// -------------------------------------------------------------
void GameObject::ToJson(json::Value& out) const
{
    out["id"]        = json::Value(m_id);
    out["name"]      = json::Value(m_name);
    out["active"]    = json::Value(m_active);
    out["parent_id"] = json::Value(m_parent ? m_parent->GetId() : static_cast<uint64_t>(0));

    // Transform 은 모든 GameObject 가 갖는 특수 Component 라 따로 기록한다.
    if (m_transform)
    {
        json::Value transformJson = json::Value::MakeObject();
        m_transform->ToJson(transformJson);
        out["transform"] = std::move(transformJson);
    }

    json::Value components = json::Value::MakeArray();
    for (const auto& bucket : m_components)
    {
        for (const auto& component : bucket.second)
        {
            if (!component || component.get() == m_transform || component->IsPendingDestroy())
                continue;

            json::Value componentJson = json::Value::MakeObject();
            component->ToJson(componentJson);
            components.Push(std::move(componentJson));
        }
    }
    out["components"] = std::move(components);
}
