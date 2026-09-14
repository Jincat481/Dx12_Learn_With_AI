#pragma once
#include "Core/stdafx.h"

class IGroundProvider;
class Scene;

// =============================================================
// GroundRaycast (S67)
//  마우스 레이가 지형과 처음 만나는 점을 찾는다.
//
//  하이트필드는 삼각형이 수만 개라 삼각형마다 교차 검사를 하기엔 너무 많다.
//  대신 "이 (x, z) 의 땅 높이" 를 물어볼 수 있다는 점을 이용한다.
//
//   1) 레이 마칭 : 레이를 조금씩 전진시키며 레이의 y 가 땅 높이 아래로 내려가는 첫 구간을 찾는다
//   2) 이분 탐색 : 그 구간(위였던 점 ~ 아래인 점)을 반씩 잘라 경계를 좁힌다
//
//  전진 폭이 너무 크면 얇은 봉우리를 건너뛰고, 너무 작으면 느리다.
//  가까운 곳은 촘촘히, 먼 곳은 성기게 전진한다(먼 곳은 화면에서 작으니 오차가 안 보인다).
// =============================================================
namespace ground
{
    bool Raycast(const IGroundProvider& ground,
                 const DirectX::XMFLOAT3& origin,
                 const DirectX::XMFLOAT3& direction,
                 float maxDistance,
                 DirectX::XMFLOAT3& outHit);

    // 씬 안의 모든 IGroundProvider 를 대상으로 쏜다. 가장 가까운 교점을 돌려준다.
    bool RaycastScene(const Scene& scene,
                      const DirectX::XMFLOAT3& origin,
                      const DirectX::XMFLOAT3& direction,
                      float maxDistance,
                      DirectX::XMFLOAT3& outHit);
}
