#pragma once
#include <cstdint>

// -------------------------------------------------------------
// IdGenerator (S19)
//  단조 증가 ID 발급기. GameObject 와 Component 는 서로 다른 계열을 쓴다.
//  Scene::Load 이후에는 EnsureAbove 로 다음 발급값을 밀어올려 ID 충돌을 막는다.
// -------------------------------------------------------------
class IdGenerator
{
public:
    static uint64_t NextGameObjectId();
    static uint64_t NextComponentId();

    static void EnsureGameObjectIdAbove(uint64_t id);
    static void EnsureComponentIdAbove(uint64_t id);

    static void Reset();

private:
    static uint64_t s_gameObjectId;
    static uint64_t s_componentId;
};
