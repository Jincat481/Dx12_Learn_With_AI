#include "Core/stdafx.h"
#include "Core/ObjectRegistry.h"

ObjectRegistry& ObjectRegistry::Get()
{
    static ObjectRegistry instance;
    return instance;
}

void ObjectRegistry::RegisterGameObject(uint64_t id, GameObject* object)
{
    if (id == 0 || object == nullptr)
        return;
    m_gameObjects[id] = object;
}

void ObjectRegistry::UnregisterGameObject(uint64_t id)
{
    m_gameObjects.erase(id);
}

void ObjectRegistry::RegisterComponent(uint64_t id, Component* component)
{
    if (id == 0 || component == nullptr)
        return;
    m_components[id] = component;
}

void ObjectRegistry::UnregisterComponent(uint64_t id)
{
    m_components.erase(id);
}

GameObject* ObjectRegistry::FindGameObject(uint64_t id) const
{
    auto it = m_gameObjects.find(id);
    return (it != m_gameObjects.end()) ? it->second : nullptr;
}

Component* ObjectRegistry::FindComponent(uint64_t id) const
{
    auto it = m_components.find(id);
    return (it != m_components.end()) ? it->second : nullptr;
}

void ObjectRegistry::Clear()
{
    m_gameObjects.clear();
    m_components.clear();
}
