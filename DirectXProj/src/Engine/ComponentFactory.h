#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"

// =============================================================
// ComponentFactory (과제 5 / S21)
//  JSON 의 "type" 문자열만 보고 알맞은 Component 를 만들어야 한다.
//  생성자를 직접 알 수 없으므로 "타입 이름 → 생성 함수" 표를 둔다.
// =============================================================
class ComponentFactory
{
public:
    using Creator = std::function<std::unique_ptr<Component>()>;

    static ComponentFactory& Get();

    void Register(const std::string& typeName, Creator creator);

    // 편의용 : 기본 생성자가 있는 타입을 한 줄로 등록한다.
    template <typename T>
    void Register(const std::string& typeName)
    {
        static_assert(std::is_base_of<Component, T>::value, "T 는 Component 를 상속해야 한다.");
        Register(typeName, []() -> std::unique_ptr<Component> { return std::make_unique<T>(); });
    }

    // 등록되지 않은 타입이면 nullptr 을 돌려주고 경고를 남긴다(로드가 중단되지 않는다).
    std::unique_ptr<Component> Create(const std::string& typeName) const;

    bool IsRegistered(const std::string& typeName) const;
    void Clear();

private:
    ComponentFactory() = default;
    ComponentFactory(const ComponentFactory&) = delete;
    ComponentFactory& operator=(const ComponentFactory&) = delete;

    std::unordered_map<std::string, Creator> m_creators;
};
