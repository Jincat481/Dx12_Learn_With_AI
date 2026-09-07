#pragma once
#include "Core/stdafx.h"
#include "Utils/Json.h"

class GameObject;
class Graphics;

// =============================================================
// Scene (과제 1, 3, 4, 5)
//  GameObject 를 소유(unique_ptr)하고 생명주기를 돌린다.
//  생성/삭제는 프레임 경계(ProcessPendingChanges)에서만 반영한다.
//  Save / Load 로 계층·Transform·Component 를 JSON 에 보존한다.
// =============================================================
class Scene
{
public:
    explicit Scene(const std::string& name = "Scene");
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    // ---- GameObject 관리 ----
    GameObject* CreateGameObject(const std::string& name = "GameObject");
    GameObject* CreateGameObjectWithId(uint64_t id, const std::string& name);

    GameObject* Find(const std::string& name) const;
    GameObject* FindById(uint64_t id) const;

    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return m_objects; }
    size_t GetGameObjectCount() const { return m_objects.size(); }

    // ---- 생명주기 ----
    void Initialize(Graphics* graphics);      // Graphics 준비 후 Component::Initialize 호출
    void Update();
    void Render();
    void ProcessPendingChanges();             // 프레임 끝 : 추가/삭제 일괄 반영
    void Clear();

    // ---- 직렬화 (과제 4, 5) ----
    bool Save(const std::wstring& path) const;
    bool Load(const std::wstring& path, Graphics* graphics);

    static constexpr int kSaveVersion = 1;

private:
    GameObject* RegisterNewObject(std::unique_ptr<GameObject> object);
    void MergePendingCreates();
    void RemoveDestroyedObjects();

    std::string m_name;
    std::vector<std::unique_ptr<GameObject>> m_objects;
    std::vector<std::unique_ptr<GameObject>> m_pendingCreate;

    Graphics* m_graphics = nullptr;
    bool m_iterating = false;
};
