#include "Core/stdafx.h"
#include "Engine/SceneManager.h"

SceneManager& SceneManager::Get()
{
    static SceneManager instance;
    return instance;
}

Scene* SceneManager::CreateScene(const std::string& name)
{
    m_activeScene = std::make_unique<Scene>(name);
    return m_activeScene.get();
}

void SceneManager::SetActiveScene(std::unique_ptr<Scene> scene)
{
    m_activeScene = std::move(scene);
}

void SceneManager::Initialize(Graphics* graphics)
{
    if (m_activeScene)
        m_activeScene->Initialize(graphics);
}

void SceneManager::Update()
{
    if (m_activeScene)
        m_activeScene->Update();
}

void SceneManager::Render()
{
    if (m_activeScene)
        m_activeScene->Render();
}

void SceneManager::ProcessPendingChanges()
{
    if (m_activeScene)
        m_activeScene->ProcessPendingChanges();
}

void SceneManager::Shutdown()
{
    m_activeScene.reset();
}
