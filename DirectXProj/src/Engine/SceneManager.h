#pragma once
#include "Core/stdafx.h"
#include "Engine/Scene.h"

// =============================================================
// SceneManager (과제 1)
//  현재 활성 Scene 하나를 소유하고 게임 루프가 호출할 진입점을 제공한다.
// =============================================================
class SceneManager
{
public:
    static SceneManager& Get();

    Scene* CreateScene(const std::string& name);   // 새 Scene 을 만들어 활성화한다
    void   SetActiveScene(std::unique_ptr<Scene> scene);
    Scene* GetActiveScene() const { return m_activeScene.get(); }

    void Initialize(Graphics* graphics);
    void Update();
    void Render();
    void ProcessPendingChanges();
    void Shutdown();

private:
    SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    std::unique_ptr<Scene> m_activeScene;
};
