#pragma once

// =============================================================
// IGroundProvider (S66)
//  "이 (x, z) 에서 땅의 높이는 얼마인가" 에 답할 수 있는 컴포넌트가 구현한다.
//
//  카메라는 지형 클래스(TerrainRenderer, ChunkedTerrainRenderer ...)를 몰라도
//  이 인터페이스 하나로 땅 위에 설 수 있다. 지형 종류가 늘어도 카메라는 그대로다.
//
//  Engine 이 Terrain 폴더에 의존하지 않도록 인터페이스는 Engine 쪽에 둔다.
//  (Terrain 은 원래 Component 때문에 Engine 에 의존하고 있다)
// =============================================================
class IGroundProvider
{
public:
    virtual ~IGroundProvider() = default;

    // 범위 밖이면 false 를 돌려준다.
    virtual bool TryGetGroundHeight(float x, float z, float& outHeight) const = 0;
};
