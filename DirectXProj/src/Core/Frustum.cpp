#include "Core/stdafx.h"
#include "Core/Frustum.h"

using namespace DirectX;

namespace
{
    // 평면 방정식을 법선 길이 1로 정규화한다.
    //  정규화해 두면 "점에서 평면까지의 거리" 를 그대로 쓸 수 있다.
    XMFLOAT4 NormalizePlane(float a, float b, float c, float d)
    {
        const float length = std::sqrt(a * a + b * b + c * c);
        if (length < 1.0e-8f)
            return XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

        const float inv = 1.0f / length;
        return XMFLOAT4(a * inv, b * inv, c * inv, d * inv);
    }
}

void Frustum::BuildFromViewProjection(FXMMATRIX viewProjection)
{
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, viewProjection);

    // 행 벡터 규약이므로 4번째 열이 w 성분이다.
    //  m._14, m._24, m._34, m._44 가 그 열이다.
    planes[Left]   = NormalizePlane(m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41);
    planes[Right]  = NormalizePlane(m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41);
    planes[Bottom] = NormalizePlane(m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42);
    planes[Top]    = NormalizePlane(m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42);

    // DirectX 의 깊이 범위는 0~1 이라 near 평면은 w 를 더하지 않는다.
    planes[Near]   = NormalizePlane(m._13,         m._23,         m._33,         m._43);
    planes[Far]    = NormalizePlane(m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43);
}

bool Frustum::IntersectsAABB(const XMFLOAT3& center, const XMFLOAT3& extents) const
{
    for (int i = 0; i < PlaneCount; ++i)
    {
        const XMFLOAT4& plane = planes[i];

        // 상자를 평면 법선 방향으로 투영했을 때의 반지름.
        //  법선 부호와 무관하게 가장 먼 꼭짓점을 고르는 것과 같다.
        const float radius = extents.x * std::fabs(plane.x) +
                             extents.y * std::fabs(plane.y) +
                             extents.z * std::fabs(plane.z);

        const float distance = plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w;

        // 상자 전체가 평면 바깥쪽에 있으면 절두체 밖이다.
        if (distance + radius < 0.0f)
            return false;
    }

    return true;
}
