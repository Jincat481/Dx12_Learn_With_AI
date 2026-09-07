#pragma once
#include "Core/stdafx.h"

// =============================================================
// Frustum (S48)
//  카메라가 실제로 볼 수 있는 절두체(잘린 피라미드)를 평면 6장으로 나타낸다.
//
//  View * Projection 행렬의 행을 더하고 빼면 6개 평면이 바로 나온다.
//  왼쪽 평면 = row4 + row1, 오른쪽 = row4 - row1 ... 같은 식이다.
//  (클립 공간에서 -w <= x <= w 인 조건을 월드 공간으로 되돌린 것이다)
//
//  평면은 ax + by + cz + d = 0 형태이고, 법선은 절두체 안쪽을 향한다.
//  따라서 어떤 점이 모든 평면에 대해 결과가 음수가 아니면 안에 있다.
// =============================================================
struct Frustum
{
    enum PlaneIndex { Left, Right, Bottom, Top, Near, Far, PlaneCount };

    DirectX::XMFLOAT4 planes[PlaneCount] = {};

    void BuildFromViewProjection(DirectX::FXMMATRIX viewProjection);

    // 축 정렬 경계 상자(AABB)가 절두체와 조금이라도 겹치는가.
    //  중심과 반지름(extents)으로 표현한다.
    bool IntersectsAABB(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& extents) const;
};
