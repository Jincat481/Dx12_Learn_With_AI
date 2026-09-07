#include "Core/stdafx.h"
#include "Engine/Scene.h"
#include "Engine/GameObject.h"
#include "Engine/ComponentFactory.h"
#include "Engine/Transform.h"
#include "Utils/IdGenerator.h"
#include "Utils/StringUtil.h"

Scene::Scene(const std::string& name)
    : m_name(name)
{
}

Scene::~Scene()
{
    Clear();
}

// -------------------------------------------------------------
// GameObject 생성 / 조회
// -------------------------------------------------------------
GameObject* Scene::RegisterNewObject(std::unique_ptr<GameObject> object)
{
    GameObject* raw = object.get();
    raw->SetScene(this);

    // Update/Render 순회 중이면 컨테이너를 건드리지 않고 대기열에 넣는다. (S16)
    if (m_iterating)
        m_pendingCreate.push_back(std::move(object));
    else
        m_objects.push_back(std::move(object));

    return raw;
}

GameObject* Scene::CreateGameObject(const std::string& name)
{
    return RegisterNewObject(std::make_unique<GameObject>(name));
}

GameObject* Scene::CreateGameObjectWithId(uint64_t id, const std::string& name)
{
    return RegisterNewObject(std::make_unique<GameObject>(id, name));
}

GameObject* Scene::Find(const std::string& name) const
{
    for (const auto& object : m_objects)
    {
        if (object && !object->IsPendingDestroy() && object->GetName() == name)
            return object.get();
    }
    for (const auto& object : m_pendingCreate)
    {
        if (object && object->GetName() == name)
            return object.get();
    }
    return nullptr;
}

GameObject* Scene::FindById(uint64_t id) const
{
    for (const auto& object : m_objects)
    {
        if (object && object->GetId() == id)
            return object.get();
    }
    for (const auto& object : m_pendingCreate)
    {
        if (object && object->GetId() == id)
            return object.get();
    }
    return nullptr;
}

// -------------------------------------------------------------
// 생명주기
// -------------------------------------------------------------
void Scene::Initialize(Graphics* graphics)
{
    m_graphics = graphics;

    MergePendingCreates();

    for (auto& object : m_objects)
    {
        if (!object)
            continue;

        object->ProcessPendingChanges(nullptr);   // 큐에 있던 Component 를 먼저 확정한다
        object->Initialize(graphics);
    }
}

void Scene::Update()
{
    m_iterating = true;

    // 인덱스로 순회한다. Update 중 새 오브젝트는 m_pendingCreate 로 가므로 안전하다.
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        GameObject* object = m_objects[i].get();
        if (object)
            object->Update();
    }

    m_iterating = false;
}

void Scene::Render()
{
    m_iterating = true;

    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        GameObject* object = m_objects[i].get();
        if (object)
            object->Render();
    }

    m_iterating = false;
}

void Scene::MergePendingCreates()
{
    if (m_pendingCreate.empty())
        return;

    auto pending = std::move(m_pendingCreate);
    m_pendingCreate.clear();

    for (auto& object : pending)
    {
        if (!object)
            continue;

        // 이미 Graphics 가 준비된 뒤에 만들어진 오브젝트도 초기화해 준다.
        if (m_graphics)
        {
            object->ProcessPendingChanges(nullptr);
            object->Initialize(m_graphics);
        }
        m_objects.push_back(std::move(object));
    }
}

void Scene::RemoveDestroyedObjects()
{
    // 삭제 예약된 오브젝트를 실제로 해제한다(소멸자에서 부모/자식 연결이 정리된다).
    m_objects.erase(
        std::remove_if(m_objects.begin(), m_objects.end(),
                       [](const std::unique_ptr<GameObject>& object)
                       {
                           if (!object)
                               return true;

                           if (!object->IsPendingDestroy())
                               return false;

                           object->DestroyAllComponents();   // OnDestroy 를 먼저 호출한다
                           return true;
                       }),
        m_objects.end());
}

void Scene::ProcessPendingChanges()
{
    // 1) 각 GameObject 의 Component 추가/삭제를 확정한다.
    for (auto& object : m_objects)
    {
        if (object)
            object->ProcessPendingChanges(m_graphics);
    }

    // 2) 이번 프레임에 만들어진 GameObject 를 편입한다.
    MergePendingCreates();

    // 3) 삭제 예약된 GameObject 를 제거한다.
    RemoveDestroyedObjects();
}

void Scene::Clear()
{
    for (auto& object : m_objects)
    {
        if (object)
            object->DestroyAllComponents();
    }
    m_objects.clear();
    m_pendingCreate.clear();
}

// =============================================================
// 저장 (과제 4)
// =============================================================
bool Scene::Save(const std::wstring& path) const
{
    json::Value root = json::Value::MakeObject();
    root["version"] = json::Value(kSaveVersion);       // 포맷 버전 관리 (S20)
    root["scene"]   = json::Value(m_name);

    json::Value objects = json::Value::MakeArray();
    for (const auto& object : m_objects)
    {
        if (!object || object->IsPendingDestroy())
            continue;

        json::Value objectJson = json::Value::MakeObject();
        object->ToJson(objectJson);
        objects.Push(std::move(objectJson));
    }
    root["gameObjects"] = std::move(objects);

    // 들여쓰기 2칸으로 사람이 읽을 수 있게 저장한다.
    if (!root.SaveToFile(path, 2))
    {
        dxutil::DebugLog(L"[Scene] 저장 실패 : %s", path.c_str());
        return false;
    }

    dxutil::DebugLog(L"[Scene] 저장 완료 : %s (GameObject %zu개)", path.c_str(), m_objects.size());
    return true;
}

// =============================================================
// 로드 (과제 5 / S22)
//  1차 순회 : GameObject 와 Component 를 만든다 (create)
//  2차 순회 : parent_id 로 부모-자식을 연결한다 (link)
//  두 단계로 나누어야 아직 만들어지지 않은 부모를 참조해도 실패하지 않는다.
// =============================================================
bool Scene::Load(const std::wstring& path, Graphics* graphics)
{
    // 파싱이 실패하면 현재 씬을 망가뜨리지 않고 그대로 둔다.
    json::Value root;
    std::string error;
    if (!json::Value::LoadFromFile(path, root, error))
    {
        dxutil::DebugLog(L"[Scene] 로드 실패 : %s (%s)", path.c_str(), StringUtil::Utf8ToWide(error).c_str());
        return false;
    }

    if (!root.IsObject())
    {
        dxutil::DebugLog(L"[Scene] 로드 실패 : 최상위가 객체가 아니다 : %s", path.c_str());
        return false;
    }

    const json::Value* objectsJson = root.Find("gameObjects");
    if (!objectsJson || !objectsJson->IsArray())
    {
        dxutil::DebugLog(L"[Scene] 로드 실패 : gameObjects 배열이 없다 : %s", path.c_str());
        return false;
    }

    if (const json::Value* version = root.Find("version"))
    {
        if (version->AsInt() != kSaveVersion)
            dxutil::DebugLog(L"[Scene] 경고 : 저장 포맷 버전이 다르다 (%d != %d)", version->AsInt(), kSaveVersion);
    }

    // 기존 씬 정리
    Clear();
    m_graphics = graphics;
    m_name = root.Find("scene") ? root.Find("scene")->AsString(m_name) : m_name;

    uint64_t maxId = 0;

    // ---------------- 1차 순회 : 생성 + 역직렬화 ----------------
    for (size_t i = 0; i < objectsJson->Size(); ++i)
    {
        const json::Value& objectJson = objectsJson->At(i);
        if (!objectJson.IsObject())
            continue;

        const uint64_t id = objectJson.Find("id") ? objectJson.Find("id")->AsUInt64() : 0;
        const std::string name = objectJson.Find("name") ? objectJson.Find("name")->AsString("GameObject") : "GameObject";

        GameObject* object = (id != 0) ? CreateGameObjectWithId(id, name) : CreateGameObject(name);
        if (!object)
            continue;

        maxId = (id > maxId) ? id : maxId;

        if (const json::Value* active = objectJson.Find("active"))
            object->SetActive(active->AsBool(true));

        // Transform 복원
        if (const json::Value* transformJson = objectJson.Find("transform"))
        {
            if (Transform* transform = object->GetTransform())
                transform->FromJson(*transformJson);
        }

        // 나머지 Component 복원
        const json::Value* componentsJson = objectJson.Find("components");
        if (!componentsJson || !componentsJson->IsArray())
            continue;

        for (size_t c = 0; c < componentsJson->Size(); ++c)
        {
            const json::Value& componentJson = componentsJson->At(c);
            if (!componentJson.IsObject())
                continue;

            const json::Value* typeJson = componentJson.Find("type");
            if (!typeJson)
            {
                dxutil::DebugLog(L"[Scene] Component 에 type 이 없다. 건너뛴다.");
                continue;
            }

            const std::string typeName = typeJson->AsString();

            // Transform 은 GameObject 가 이미 갖고 있으므로 데이터만 덮어쓴다.
            if (typeName == "Transform")
            {
                if (Transform* transform = object->GetTransform())
                    transform->FromJson(componentJson);
                continue;
            }

            std::unique_ptr<Component> component = ComponentFactory::Get().Create(typeName);
            if (!component)
                continue;    // 등록되지 않은 타입 : 경고는 Factory 가 남긴다

            component->FromJson(componentJson);
            object->AttachComponentImmediate(std::move(component));
        }
    }

    // 1차에서 만든 오브젝트들을 실제 목록으로 옮긴다(아직 Initialize 는 하지 않는다).
    for (auto& object : m_pendingCreate)
        m_objects.push_back(std::move(object));
    m_pendingCreate.clear();

    // ---------------- 2차 순회 : 부모-자식 연결 ----------------
    for (size_t i = 0; i < objectsJson->Size(); ++i)
    {
        const json::Value& objectJson = objectsJson->At(i);
        if (!objectJson.IsObject())
            continue;

        const json::Value* idJson = objectJson.Find("id");
        const json::Value* parentJson = objectJson.Find("parent_id");
        if (!idJson || !parentJson)
            continue;

        const uint64_t parentId = parentJson->AsUInt64();
        if (parentId == 0)
            continue;    // 루트 오브젝트

        GameObject* child = FindById(idJson->AsUInt64());
        GameObject* parent = FindById(parentId);

        if (!child)
            continue;

        if (!parent)
        {
            dxutil::DebugLog(L"[Scene] 경고 : parent_id %llu 를 찾을 수 없다. 루트로 둔다.", parentId);
            continue;
        }

        // 로컬 값은 이미 JSON 에서 복원했으므로 월드 유지 보정을 하면 안 된다.
        child->SetParent(parent, /*worldPositionStays*/ false);
    }

    // ---------------- ID 생성기 갱신 ----------------
    IdGenerator::EnsureGameObjectIdAbove(maxId);

    // ---------------- 초기화 순서 : Deserialize → GPU Initialize → Start ----------------
    if (graphics)
    {
        for (auto& object : m_objects)
        {
            if (object)
                object->Initialize(graphics);
        }
    }
    // Start 는 각 GameObject 의 첫 Update 에서 호출된다.

    dxutil::DebugLog(L"[Scene] 로드 완료 : %s (GameObject %zu개)", path.c_str(), m_objects.size());
    return true;
}
