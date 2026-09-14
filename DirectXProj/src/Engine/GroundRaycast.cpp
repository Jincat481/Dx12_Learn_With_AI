#include "Core/stdafx.h"
#include "Engine/GroundRaycast.h"
#include "Engine/GroundProvider.h"
#include "Engine/Scene.h"
#include "Engine/GameObject.h"

using namespace DirectX;

namespace ground
{
    namespace
    {
        XMFLOAT3 PointAt(const XMFLOAT3& origin, const XMFLOAT3& direction, float t)
        {
            return XMFLOAT3(origin.x + direction.x * t,
                            origin.y + direction.y * t,
                            origin.z + direction.z * t);
        }

        // 레이 위의 점이 땅보다 위에 있는가. 지형 범위 밖은 "위" 로 본다(가로막는 것이 없다).
        bool IsAbove(const IGroundProvider& ground, const XMFLOAT3& point, float& outGround)
        {
            if (!ground.TryGetGroundHeight(point.x, point.z, outGround))
                return true;

            return point.y > outGround;
        }
    }

    bool Raycast(const IGroundProvider& ground,
                 const XMFLOAT3& origin,
                 const XMFLOAT3& direction,
                 float maxDistance,
                 XMFLOAT3& outHit)
    {
        float groundHeight = 0.0f;

        // 시작점이 이미 땅속이면 "처음 만나는 점" 이 정의되지 않는다.
        if (!IsAbove(ground, origin, groundHeight))
            return false;

        float previousT = 0.0f;
        float t = 0.0f;

        while (t < maxDistance)
        {
            // 가까운 곳은 0.5 단위로, 멀수록 넓게 전진한다.
            t = (std::min)(maxDistance, t + 0.5f + t * 0.004f);

            if (IsAbove(ground, PointAt(origin, direction, t), groundHeight))
            {
                previousT = t;
                continue;
            }

            // previousT 는 땅 위, t 는 땅 아래. 경계가 이 사이에 있다.
            float low = previousT;
            float high = t;

            for (int i = 0; i < 16; ++i)
            {
                const float middle = (low + high) * 0.5f;
                if (IsAbove(ground, PointAt(origin, direction, middle), groundHeight))
                    low = middle;
                else
                    high = middle;
            }

            outHit = PointAt(origin, direction, high);

            // 표면 위에 정확히 올려 둔다 (이분 탐색이 남긴 아주 작은 오차 제거)
            if (ground.TryGetGroundHeight(outHit.x, outHit.z, groundHeight))
                outHit.y = groundHeight;

            return true;
        }

        return false;
    }

    bool RaycastScene(const Scene& scene,
                      const XMFLOAT3& origin,
                      const XMFLOAT3& direction,
                      float maxDistance,
                      XMFLOAT3& outHit)
    {
        bool found = false;
        float nearest = maxDistance;

        for (const auto& object : scene.GetGameObjects())
        {
            if (!object || object->IsPendingDestroy() || !object->IsActive())
                continue;

            for (const auto& entry : object->GetComponentMap())
            {
                for (const auto& component : entry.second)
                {
                    const IGroundProvider* provider = dynamic_cast<const IGroundProvider*>(component.get());
                    if (!provider || component->IsPendingDestroy() || !component->IsEnabled())
                        continue;

                    XMFLOAT3 hit{};
                    if (!Raycast(*provider, origin, direction, nearest, hit))
                        continue;

                    const float dx = hit.x - origin.x;
                    const float dy = hit.y - origin.y;
                    const float dz = hit.z - origin.z;
                    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

                    if (distance <= nearest)
                    {
                        nearest = distance;
                        outHit = hit;
                        found = true;
                    }
                }
            }
        }

        return found;
    }
}
