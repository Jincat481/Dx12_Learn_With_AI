#include "Core/stdafx.h"
#include "Utils/IdGenerator.h"

uint64_t IdGenerator::s_gameObjectId = 0;
uint64_t IdGenerator::s_componentId = 0;

uint64_t IdGenerator::NextGameObjectId()
{
    return ++s_gameObjectId;   // 0 은 "없음"(부모 없음)을 뜻하므로 1부터 시작한다.
}

uint64_t IdGenerator::NextComponentId()
{
    return ++s_componentId;
}

void IdGenerator::EnsureGameObjectIdAbove(uint64_t id)
{
    if (s_gameObjectId < id)
        s_gameObjectId = id;
}

void IdGenerator::EnsureComponentIdAbove(uint64_t id)
{
    if (s_componentId < id)
        s_componentId = id;
}

void IdGenerator::Reset()
{
    s_gameObjectId = 0;
    s_componentId = 0;
}
