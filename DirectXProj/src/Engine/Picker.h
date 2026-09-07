#pragma once
#include "Core/stdafx.h"

class Scene;
class GameObject;

// =============================================================
// Picker
//  2D 피킹. 화면에서 찍은 월드 좌표로 Scene 안의 GameObject 를 고른다.
//
//  겹친 경우에는 "나중에 그려진 것"이 위에 보이므로,
//  Scene 의 렌더 순서에서 가장 뒤에 있는 히트를 선택한다.
//  (깊이 버퍼가 없는 2D 라 화면에 보이는 것과 결과가 일치한다.)
// =============================================================
class Picker
{
public:
    // 맞은 것이 없으면 nullptr.
    static GameObject* Pick(Scene& scene, const DirectX::XMFLOAT3& worldPoint);
};
