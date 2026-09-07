#include "Core/stdafx.h"
#include "Core/Game.h"
#include "Core/Window.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Core/ObjectRegistry.h"

#include "Engine/Scene.h"
#include "Engine/SceneManager.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Engine/SpriteRenderer.h"
#include "Engine/ComponentFactory.h"
#include "Engine/Picker.h"
#include "Engine/Camera.h"

#include "Terrain/TerrainRenderer.h"
#include "Editor/MenuScreen.h"

#include "Graphics/ShaderManager.h"
#include "Graphics/TextureManager.h"

#include "Game/PlayerController.h"

#include "Editor/EditorStyle.h"

#include "Input/InputManager.h"
#include "Utils/Paths.h"

#include <cstdio>

using namespace DirectX;

Game::Game() = default;

Game::~Game()
{
    Shutdown();
}

bool Game::Initialize(HINSTANCE hInstance, int width, int height)
{
    // WIC 를 쓰려면 COM 초기화가 먼저다.
    const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
        m_comInitialized = true;
    else if (hr != RPC_E_CHANGED_MODE)
        return DX_CHECK(hr, L"CoInitializeEx");

    // 1) 창
    m_window = std::make_unique<Window>();
    if (!m_window->Create(hInstance, L"DirectXProj", width, height))
        return false;

    // 2) 그래픽
    m_graphics = std::make_unique<Graphics>();
    if (!m_graphics->Initialize(m_window->GetHandle(), width, height))
    {
        ::MessageBoxW(m_window->GetHandle(),
                      L"Direct3D 11 초기화에 실패했다.\nShaders 폴더의 HLSL 파일이 있는지 확인한다.",
                      L"DirectXProj", MB_OK | MB_ICONERROR);
        return false;
    }

    // 3) 시간
    TimeManager::Get().Initialize();

    // 4) Component 타입 등록 (Scene 로드보다 먼저)
    RegisterComponentTypes();

    // 5) 씬 구성
    m_savePath = Paths::ResolveForWrite(L"Saves/scene.json");

    // 5) 메뉴로 시작한다. 씬은 항목을 고른 뒤에 만든다.
    SetupMenu();
    m_state = AppState::Menu;

    m_running = true;

    dxutil::DebugLog(L"[Game] 초기화 완료");
    dxutil::DebugLog(L"  [터레인] 가운데버튼 드래그 궤도회전 | 휠 줌 | WASD 이동 | Q,E 높이 | F 리셋 | G 와이어프레임");
    dxutil::DebugLog(L"  [터레인] G 와이어프레임 | T 높이 켜기/끄기 | N 새 지형");
    dxutil::DebugLog(L"  [공통] ESC 메뉴로 | H Hierarchy | I Inspector");
    dxutil::DebugLog(L"  좌클릭 선택 | 좌드래그 이동 | 우클릭 해제");
    dxutil::DebugLog(L"  H: Hierarchy 켜기/끄기 | I: Inspector 켜기/끄기");
    dxutil::DebugLog(L"  Hierarchy 에서 줄을 끌어다 다른 줄 위에 놓으면 그 자식이 되고, 아래 빈 곳에 놓으면 루트로 분리된다");
    dxutil::DebugLog(L"  F5 저장 | F9 불러오기 | F1 자식 추가 | F2 자식 삭제 | ESC 종료");
    dxutil::DebugLog(L"  저장 경로 : %s", m_savePath.c_str());
    return true;
}

// -------------------------------------------------------------
// 과제 5 : 로드 가능한 Component 타입을 Factory 에 등록한다.
// -------------------------------------------------------------
void Game::RegisterComponentTypes()
{
    ComponentFactory& factory = ComponentFactory::Get();

    factory.Register<Transform>("Transform");
    factory.Register<SpriteRenderer>("SpriteRenderer");
    factory.Register<PlayerController>("PlayerController");
    factory.Register<Camera>("Camera");
    factory.Register<TerrainRenderer>("TerrainRenderer");
}

// -------------------------------------------------------------
// 터레인 쇼케이스 - 스텝 1 : 평면 그리드
//  카메라 오브젝트 하나와 터레인 오브젝트 하나로 시작한다.
// -------------------------------------------------------------
void Game::BuildTerrainScene()
{
    Scene* scene = SceneManager::Get().CreateScene("TerrainShowcase");
    if (!scene)
        return;

    GameObject* cameraObject = scene->CreateGameObject("MainCamera");
    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetTarget(XMFLOAT3(0.0f, 0.0f, 0.0f));
    camera->SetDistance(90.0f);
    camera->SetAngles(35.0f, 30.0f);

    GameObject* terrainObject = scene->CreateGameObject("Terrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(64, 64, 2.0f);   // 64 x 64 칸, 칸 한 변 2 → 128 x 128 크기
}

void Game::BuildSpriteDemoScene()
{
    Scene* scene = SceneManager::Get().CreateScene("SpriteDemo");
    if (!scene)
        return;

    // 부모 : 조작 대상
    GameObject* parent = scene->CreateGameObject("Player");
    parent->AddComponent<SpriteRenderer>(L"Assets/player.png");
    parent->AddComponent<PlayerController>();
    parent->GetTransform()->SetLocalPosition(0.0f, 0.0f, 0.0f);

    // 자식 : 부모를 따라 움직이는지 확인한다.
    GameObject* child = scene->CreateGameObject("Child");
    child->AddComponent<SpriteRenderer>(L"Assets/child.png");
    child->GetTransform()->SetLocalPosition(180.0f, 0.0f, 0.0f);
    child->GetTransform()->SetLocalScale(0.6f, 0.6f, 1.0f);
    child->SetParent(parent, /*worldPositionStays*/ false);

    // 손자 : 계층이 2단 이상 전파되는지 확인한다.
    GameObject* grandChild = scene->CreateGameObject("GrandChild");
    grandChild->AddComponent<SpriteRenderer>(L"Assets/child.png");
    grandChild->GetTransform()->SetLocalPosition(150.0f, 0.0f, 0.0f);
    grandChild->GetTransform()->SetLocalScale(0.6f, 0.6f, 1.0f);
    grandChild->SetParent(child, /*worldPositionStays*/ false);
}

int Game::Run()
{
    if (!m_running)
        return -1;

    while (m_window->ProcessMessages())        // 1. 메시지 처리
    {
        TimeManager::Get().Update();
        InputManager::Get().Update();          // 2. 입력 갱신

        if (m_state == AppState::Menu)
            UpdateMenuFrame();
        else
            UpdateShowcaseFrame();
    }

    return 0;
}

// -------------------------------------------------------------
// 메뉴 상태 : 씬을 돌리지 않고 메뉴만 그린다.
// -------------------------------------------------------------
void Game::SetupMenu()
{
    m_menu.SetEntries({
        { L"터레인 쇼케이스",   L"절차적 하이트맵 지형 · 궤도 카메라 · 와이어프레임 (스텝 1~2)" },
        { L"2D 스프라이트 데모", L"계층 Transform · 피킹 · 드래그 · Hierarchy / Inspector" },
        { L"종료",             L"프로그램을 끝낸다" },
    });
}

void Game::UpdateMenuFrame()
{
    InputManager& input = InputManager::Get();

    const int choice = m_menu.Update(input, m_graphics->GetWidth(), m_graphics->GetHeight());

    m_graphics->BeginFrame();
    if (HDC hdc = m_graphics->BeginOverlay())
    {
        m_menu.Draw(hdc, m_graphics->GetWidth(), m_graphics->GetHeight());
        m_graphics->EndOverlay();
    }
    m_graphics->EndFrame();

    if (input.GetKeyDown(VK_ESCAPE))
    {
        ::PostQuitMessage(0);
        return;
    }

    if (choice != MenuScreen::kNoSelection)
        EnterShowcase(choice);

    UpdateWindowTitle();
}

void Game::EnterShowcase(int index)
{
    // 마지막 항목은 종료
    if (index == 2)
    {
        ::PostQuitMessage(0);
        return;
    }

    m_selectedId = 0;
    m_dragging = false;
    m_currentShowcase = index;

    if (index == 0)
        BuildTerrainScene();
    else
        BuildSpriteDemoScene();

    SceneManager::Get().Initialize(m_graphics.get());
    m_state = AppState::Showcase;

    dxutil::DebugLog(L"[Game] 쇼케이스 진입 : %d (ESC 로 메뉴 복귀)", index);
}

void Game::ReturnToMenu()
{
    m_selectedId = 0;
    m_dragging = false;
    m_currentShowcase = -1;

    SceneManager::Get().Shutdown();
    m_state = AppState::Menu;

    dxutil::DebugLog(L"[Game] 메뉴로 복귀");
}

// -------------------------------------------------------------
// 쇼케이스 상태 : 기존 게임 루프 그대로.
//   입력 갱신 → 에디터 UI → 피킹 → 씬 Update
//   → BeginFrame → 씬 Render → 오버레이 → EndFrame → 프레임 끝 정리
// -------------------------------------------------------------
void Game::UpdateShowcaseFrame()
{
    TimeManager& time = TimeManager::Get();
    SceneManager& sceneManager = SceneManager::Get();

    UpdateEditorUI();                      // Hierarchy / Inspector
    UpdatePickingAndDrag();                // 피킹 / 하이라이트 / 드래그

#if HOT_RELOAD_ENABLED
    ShaderManager::Get().Update(time.GetDeltaTime());
#endif

    sceneManager.Update();

    m_graphics->BeginFrame();
    sceneManager.Render();
    DrawEditorUI();
    m_graphics->EndFrame();

    sceneManager.ProcessPendingChanges();
    HandleFrameEndCommands();
    UpdateWindowTitle();
}

GameObject* Game::GetSelectedObject() const
{
    if (m_selectedId == 0)
        return nullptr;

    GameObject* object = ObjectRegistry::Get().FindGameObject(m_selectedId);
    return (object && !object->IsPendingDestroy()) ? object : nullptr;
}

// -------------------------------------------------------------
// 에디터 UI 입력
//  패널이 먼저 입력을 가져가고, 그 결과를 Scene 에 반영한다.
//  인스펙터가 글자를 받는 동안에는 게임 쪽 키 입력을 막는다.
// -------------------------------------------------------------
void Game::UpdateEditorUI()
{
    Scene* scene = SceneManager::Get().GetActiveScene();
    if (!scene || !m_graphics)
        return;

    InputManager& input = InputManager::Get();

    // 패널 켜고 끄기 (편집 중에는 GetKeyDown 이 막히므로 글자가 새지 않는다)
    if (input.GetKeyDown('H')) m_hierarchy.Toggle();
    if (input.GetKeyDown('I')) m_inspector.Toggle();

    // ---- Hierarchy ----
    const HierarchyPanel::Action action = m_hierarchy.Update(*scene, input, m_selectedId);

    if (action.selectId != 0)
    {
        m_selectedId = action.selectId;
        m_dragging = false;              // 패널에서 고른 것은 월드 드래그가 아니다
    }

    if (action.reparent)
        ApplyReparent(action.dragId, action.newParentId);

    // ---- Inspector ----
    m_inspector.Update(input, GetSelectedObject(), m_graphics->GetWidth());

    // 글자를 입력받는 동안 WASD 로 캐릭터가 움직이면 안 된다.
    input.SetTextCaptureActive(m_inspector.IsEditing());
}

void Game::DrawEditorUI()
{
    if (!m_graphics)
        return;

    // 오버레이 DC 는 프레임당 한 번만 잡고 두 패널이 나눠 쓴다.
    HDC hdc = m_graphics->BeginOverlay();
    if (!hdc)
        return;

    m_hierarchy.Draw(hdc, m_selectedId);
    m_inspector.Draw(hdc);

    m_graphics->EndOverlay();
}

// -------------------------------------------------------------
// Hierarchy 드래그로 요청된 부모 변경을 실제로 적용한다.
//  newParentId == 0 이면 부모에서 떼어 루트로 만든다.
//  worldPositionStays = true 라 화면에 보이는 모습은 그대로다. (과제 3)
// -------------------------------------------------------------
void Game::ApplyReparent(uint64_t dragId, uint64_t newParentId)
{
    GameObject* dragged = ObjectRegistry::Get().FindGameObject(dragId);
    if (!dragged || dragged->IsPendingDestroy())
        return;

    GameObject* newParent = nullptr;
    if (newParentId != 0)
    {
        newParent = ObjectRegistry::Get().FindGameObject(newParentId);
        if (!newParent || newParent->IsPendingDestroy())
            return;
    }

    if (newParent == dragged->GetParent())
        return;

    // 자기 자손을 부모로 삼는 순환은 SetParent 가 걸러 낸다.
    dragged->SetParent(newParent, /*worldPositionStays*/ true);

    if (newParent)
    {
        dxutil::DebugLog(L"[Hierarchy] %S → %S 의 자식으로 이동",
                         dragged->GetName().c_str(), newParent->GetName().c_str());
    }
    else
    {
        dxutil::DebugLog(L"[Hierarchy] %S 을(를) 루트로 분리", dragged->GetName().c_str());
    }
}

// -------------------------------------------------------------
// 마우스 피킹 / 선택 하이라이트 / 드래그 이동
//  왼쪽 클릭      : 커서 아래의 스프라이트를 고른다(겹치면 위에 그려진 쪽)
//  왼쪽 누른 채 이동 : 고른 오브젝트를 커서를 따라 옮긴다
//  오른쪽 클릭 / 빈 곳 클릭 : 선택 해제
// -------------------------------------------------------------
void Game::UpdatePickingAndDrag()
{
    Scene* scene = SceneManager::Get().GetActiveScene();
    if (!scene || !m_graphics)
        return;

    InputManager& input = InputManager::Get();

    // 화면 좌표 → 월드 좌표 (z = 0 평면)
    const XMFLOAT3 worldPoint = m_graphics->ScreenToWorld(input.GetMouseX(), input.GetMouseY());

    // 패널 위에서 누른 클릭은 에디터 UI 가 이미 처리했다. 월드 피킹으로 넘기지 않는다.
    const bool overPanel = m_hierarchy.Contains(input.GetMouseX(), input.GetMouseY()) ||
                           m_inspector.Contains(input.GetMouseX(), input.GetMouseY());

    // ---- 누른 순간 : 피킹 + 드래그 시작 ----
    if (input.GetMouseButtonDown(InputManager::Left) && !overPanel)
    {
        GameObject* picked = Picker::Pick(*scene, worldPoint);

        m_selectedId = picked ? picked->GetId() : 0;
        m_dragging = false;

        if (picked)
        {
            if (Transform* transform = picked->GetTransform())
            {
                // 잡은 지점이 튀지 않도록 "오브젝트 위치 - 커서 위치" 를 기억한다.
                const XMFLOAT3 objectWorld = transform->GetWorldPosition();
                m_dragOffset = XMFLOAT3(objectWorld.x - worldPoint.x, objectWorld.y - worldPoint.y, 0.0f);
                m_dragging = true;
            }

            dxutil::DebugLog(L"[Picking] 선택 : %S (id=%llu) / 월드(%.1f, %.1f)",
                             picked->GetName().c_str(), picked->GetId(), worldPoint.x, worldPoint.y);
        }
        else
        {
            dxutil::DebugLog(L"[Picking] 선택 해제 / 월드(%.1f, %.1f)", worldPoint.x, worldPoint.y);
        }
    }

    // ---- 누르고 있는 동안 : 커서를 따라 이동 ----
    if (m_dragging && input.GetMouseButton(InputManager::Left) && m_selectedId != 0)
    {
        GameObject* target = ObjectRegistry::Get().FindGameObject(m_selectedId);

        if (target && !target->IsPendingDestroy() && target->GetTransform())
        {
            Transform* transform = target->GetTransform();

            // z 는 건드리지 않는다(2D 이동).
            const XMFLOAT3 currentWorld = transform->GetWorldPosition();
            transform->SetWorldPosition(XMFLOAT3(worldPoint.x + m_dragOffset.x,
                                                 worldPoint.y + m_dragOffset.y,
                                                 currentWorld.z));
        }
        else
        {
            m_dragging = false;   // 드래그 중 대상이 사라진 경우
        }
    }

    // ---- 버튼을 떼는 순간 ----
    //  부모-자식 관계는 건드리지 않는다. 계층을 바꾸려면 Hierarchy 패널에서 끌어다 놓는다.
    if (m_dragging && !input.GetMouseButton(InputManager::Left))
        m_dragging = false;

    if (input.GetMouseButtonDown(InputManager::Right) && !overPanel)
    {
        m_selectedId = 0;
        m_dragging = false;
    }

    // 하이라이트 상태를 매 프레임 다시 칠한다.
    // ID 로 비교하므로 선택한 오브젝트가 F2 로 삭제되면 선택이 저절로 풀린다.
    bool stillAlive = false;

    for (const auto& object : scene->GetGameObjects())
    {
        if (!object)
            continue;

        const bool selected = (m_selectedId != 0) && (object->GetId() == m_selectedId) && !object->IsPendingDestroy();
        if (selected)
            stillAlive = true;

        for (SpriteRenderer* renderer : object->GetComponents<SpriteRenderer>())
        {
            if (renderer)
                renderer->SetSelected(selected);
        }
    }

    if (!stillAlive)
        m_selectedId = 0;
}

// -------------------------------------------------------------
// Scene 을 통째로 갈아엎는 작업(로드)은 순회 중에 하면 안 된다.
// 그래서 프레임이 완전히 끝난 뒤에 처리한다.
// -------------------------------------------------------------
void Game::HandleFrameEndCommands()
{
    InputManager& input = InputManager::Get();
    Scene* scene = SceneManager::Get().GetActiveScene();
    if (!scene)
        return;

    // ESC : 메뉴로 복귀 (인스펙터 편집 중이면 Esc 는 편집 취소로 이미 쓰였다)
    if (input.GetKeyDown(VK_ESCAPE) && !m_inspector.IsEditing())
    {
        ReturnToMenu();
        return;
    }

    // F5 : 저장
    if (input.GetKeyDown(VK_F5))
    {
        if (scene->Save(m_savePath))
            dxutil::DebugLog(L"[Game] 저장 완료 → %s", m_savePath.c_str());
        else
            dxutil::DebugLog(L"[Game] 저장 실패");
    }

    // F9 : 불러오기
    if (input.GetKeyDown(VK_F9))
    {
        if (!scene->Load(m_savePath, m_graphics.get()))
            dxutil::DebugLog(L"[Game] 불러오기 실패 (먼저 F5 로 저장한다)");
    }

    // 터레인 조작 (터레인 쇼케이스에서만 의미가 있다)
    for (const auto& object : scene->GetGameObjects())
    {
        if (!object) continue;

        TerrainRenderer* terrain = object->GetComponent<TerrainRenderer>();
        if (!terrain) continue;

        if (input.GetKeyDown('G'))          // 와이어프레임
            terrain->SetWireframe(!terrain->IsWireframe());

        if (input.GetKeyDown('T'))          // 평면 <-> 하이트맵
        {
            terrain->SetHeightEnabled(!terrain->IsHeightEnabled());
            dxutil::DebugLog(L"[Terrain] 높이 %s", terrain->IsHeightEnabled() ? L"켬" : L"끔");
        }

        if (input.GetKeyDown('N'))          // 새 지형 생성
        {
            const unsigned seed = static_cast<unsigned>(TimeManager::Get().GetFrameCount() * 2654435761u + 12345u);
            terrain->Regenerate(seed);
            dxutil::DebugLog(L"[Terrain] 새 seed 로 재생성 : %u", seed);
        }
    }

    // F1 : 자식 GameObject 추가 (지연 생성 확인)
    if (input.GetKeyDown(VK_F1))
    {
        if (GameObject* player = scene->Find("Player"))
        {
            GameObject* extra = scene->CreateGameObject("Extra");
            extra->AddComponent<SpriteRenderer>(L"Assets/child.png");

            const float angle = static_cast<float>(player->GetChildren().size()) * 1.2f;
            extra->GetTransform()->SetLocalPosition(std::cos(angle) * 260.0f, std::sin(angle) * 260.0f, 0.0f);
            extra->GetTransform()->SetLocalScale(0.5f, 0.5f, 1.0f);
            extra->SetParent(player, false);

            dxutil::DebugLog(L"[Game] Extra 추가 (자식 %zu개)", player->GetChildren().size());
        }
    }

    // F2 : 마지막 자식 삭제 (지연 삭제 확인)
    if (input.GetKeyDown(VK_F2))
    {
        if (GameObject* player = scene->Find("Player"))
        {
            const auto& children = player->GetChildren();
            if (!children.empty())
            {
                GameObject* last = children.back();
                dxutil::DebugLog(L"[Game] 삭제 예약 : %S", last->GetName().c_str());
                last->Destroy();
            }
        }
    }
}

void Game::UpdateWindowTitle()
{
    TimeManager& time = TimeManager::Get();

    m_titleTimer += time.GetDeltaTime();
    if (m_titleTimer < 0.5f)
        return;
    m_titleTimer = 0.0f;

    Scene* scene = SceneManager::Get().GetActiveScene();

    // 선택된 오브젝트 이름을 제목에 같이 보여준다.
    std::string selectedName = "-";
    if (m_selectedId != 0)
    {
        if (GameObject* selected = ObjectRegistry::Get().FindGameObject(m_selectedId))
            selectedName = selected->GetName();
    }

    wchar_t title[256];
    _snwprintf_s(title, _countof(title), _TRUNCATE,
                 L"DirectXProj | FPS %.0f | dt %.3fms | GameObject %zu | Component %zu | 선택 %S",
                 time.GetFPS(),
                 time.GetDeltaTime() * 1000.0f,
                 scene ? scene->GetGameObjectCount() : 0,
                 ObjectRegistry::Get().GetComponentCount(),
                 selectedName.c_str());

    m_window->SetTitle(title);
}

void Game::Shutdown()
{
    if (!m_window && !m_graphics)
        return;

    editor::ReleaseUIFont();

    SceneManager::Get().Shutdown();
    ShaderManager::Get().Shutdown();
    TextureManager::Get().Shutdown();
    ObjectRegistry::Get().Clear();
    ComponentFactory::Get().Clear();

    m_graphics.reset();
    m_window.reset();

    if (m_comInitialized)
    {
        ::CoUninitialize();
        m_comInitialized = false;
    }

    m_running = false;
}
