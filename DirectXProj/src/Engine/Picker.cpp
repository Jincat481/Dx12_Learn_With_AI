#include "Core/stdafx.h"
#include "Engine/Picker.h"
#include "Engine/Scene.h"
#include "Engine/GameObject.h"
#include "Engine/SpriteRenderer.h"

using namespace DirectX;

GameObject* Picker::Pick(Scene& scene, const XMFLOAT3& worldPoint)
{
    GameObject* picked = nullptr;

    // 앞에서부터 훑되 히트할 때마다 갱신한다 →
    // 마지막(=가장 위에 그려진) 히트가 남는다.
    for (const auto& object : scene.GetGameObjects())
    {
        if (!object || !object->IsActive() || object->IsPendingDestroy())
            continue;

        // 같은 오브젝트에 SpriteRenderer 가 여러 개 붙어 있을 수 있다. (과제 4)
        for (SpriteRenderer* renderer : object->GetComponents<SpriteRenderer>())
        {
            if (!renderer || !renderer->IsEnabled())
                continue;

            if (renderer->HitTest(worldPoint))
            {
                picked = object.get();
                break;
            }
        }
    }

    return picked;
}
