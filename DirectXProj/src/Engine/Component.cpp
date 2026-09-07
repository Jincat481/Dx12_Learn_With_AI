#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Engine/GameObject.h"
#include "Core/ObjectRegistry.h"
#include "Utils/IdGenerator.h"

Component::Component()
    : m_id(IdGenerator::NextComponentId())
{
    ObjectRegistry::Get().RegisterComponent(m_id, this);
}

Component::~Component()
{
    ObjectRegistry::Get().UnregisterComponent(m_id);
}

void Component::SetId(uint64_t id)
{
    if (id == 0 || id == m_id)
        return;

    ObjectRegistry::Get().UnregisterComponent(m_id);
    m_id = id;
    ObjectRegistry::Get().RegisterComponent(m_id, this);

    // 이후 자동 발급 ID 가 겹치지 않게 한다.
    IdGenerator::EnsureComponentIdAbove(id);
}

Transform* Component::GetTransform() const
{
    return m_owner ? m_owner->GetTransform() : nullptr;
}

void Component::ToJson(json::Value& out) const
{
    out["type"]    = json::Value(std::string(GetTypeName()));
    out["id"]      = json::Value(m_id);
    out["enabled"] = json::Value(m_enabled);
}

void Component::FromJson(const json::Value& in)
{
    if (const json::Value* id = in.Find("id"))
        SetId(id->AsUInt64());

    if (const json::Value* enabled = in.Find("enabled"))
        m_enabled = enabled->AsBool(true);
}
