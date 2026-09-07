#include "Core/stdafx.h"
#include "Engine/ComponentFactory.h"
#include "Utils/StringUtil.h"

ComponentFactory& ComponentFactory::Get()
{
    static ComponentFactory instance;
    return instance;
}

void ComponentFactory::Register(const std::string& typeName, Creator creator)
{
    if (typeName.empty() || !creator)
        return;

    m_creators[typeName] = std::move(creator);
}

bool ComponentFactory::IsRegistered(const std::string& typeName) const
{
    return m_creators.find(typeName) != m_creators.end();
}

std::unique_ptr<Component> ComponentFactory::Create(const std::string& typeName) const
{
    auto it = m_creators.find(typeName);
    if (it == m_creators.end())
    {
        dxutil::DebugLog(L"[ComponentFactory] 등록되지 않은 Component 타입 : %s",
                         StringUtil::Utf8ToWide(typeName).c_str());
        return nullptr;
    }

    return it->second();
}

void ComponentFactory::Clear()
{
    m_creators.clear();
}
