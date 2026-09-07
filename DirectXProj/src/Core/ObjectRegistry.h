#pragma once
#include "Core/stdafx.h"

class GameObject;
class Component;

// =============================================================
// ObjectRegistry (과제 4 / S19)
//  ID → 객체 포인터 조회소.
//  등록/해제 시점을 객체의 생성/소멸과 일치시켜
//  "이미 삭제된 객체가 조회되는" 상황을 막는다.
// =============================================================
class ObjectRegistry
{
public:
    static ObjectRegistry& Get();

    void RegisterGameObject(uint64_t id, GameObject* object);
    void UnregisterGameObject(uint64_t id);

    void RegisterComponent(uint64_t id, Component* component);
    void UnregisterComponent(uint64_t id);

    GameObject* FindGameObject(uint64_t id) const;
    Component*  FindComponent(uint64_t id) const;

    void Clear();

    size_t GetGameObjectCount() const { return m_gameObjects.size(); }
    size_t GetComponentCount()  const { return m_components.size(); }

private:
    ObjectRegistry() = default;
    ObjectRegistry(const ObjectRegistry&) = delete;
    ObjectRegistry& operator=(const ObjectRegistry&) = delete;

    std::unordered_map<uint64_t, GameObject*> m_gameObjects;
    std::unordered_map<uint64_t, Component*>  m_components;
};
