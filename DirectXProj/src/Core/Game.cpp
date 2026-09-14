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
#include "Terrain/ChunkedTerrainRenderer.h"
#include "Engine/SkyRenderer.h"
#include "Engine/WaterRenderer.h"
#include "Terrain/TessellatedTerrainRenderer.h"
#include "Terrain/TerrainBrush.h"
#include "Terrain/TerrainErosion.h"
#include "Editor/MenuScreen.h"
#include "Game/MenuController.h"

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

    // 5) 메뉴도 씬이다. 메뉴 씬을 만들어 시작한다.
    SetupShowcaseList();
    BuildMenuScene();
    SceneManager::Get().Initialize(m_graphics.get());

    m_running = true;

    dxutil::DebugLog(L"[Game] 초기화 완료");
    dxutil::DebugLog(L"  [카메라] 우클릭 누른 채 : 마우스 회전 | WASD 이동 | Q,E 상하 | 휠 속도조절");
    dxutil::DebugLog(L"           F : 기본 위치로 리셋");
    dxutil::DebugLog(L"  [터레인] Tab 표시 모드 순환 | N 새 지형 | P 펄린<->값 노이즈 | T 트라이플래너 | G 걷기/비행");
    dxutil::DebugLog(L"           O 안개 | B 백그라운드 청크 생성 (스텝 10)");
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
    factory.Register<ChunkedTerrainRenderer>("ChunkedTerrainRenderer");
    factory.Register<SkyRenderer>("SkyRenderer");
    factory.Register<TessellatedTerrainRenderer>("TessellatedTerrainRenderer");
    factory.Register<TerrainBrush>("TerrainBrush");
    factory.Register<TerrainErosion>("TerrainErosion");
    factory.Register<WaterRenderer>("WaterRenderer");
    factory.Register<MenuController>("MenuController");
}

// -------------------------------------------------------------
// 터레인 쇼케이스 - 스텝 1 : 평면 그리드
//  카메라 오브젝트 하나와 터레인 오브젝트 하나로 시작한다.
// -------------------------------------------------------------
void Game::BuildTerrainScene(TerrainMode mode, const std::string& sceneName)
{
    Scene* scene = SceneManager::Get().CreateScene(sceneName);
    if (!scene)
        return;

    GameObject* cameraObject = scene->CreateGameObject("MainCamera");
    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetPosition(XMFLOAT3(0.0f, 60.0f, -90.0f));
    camera->LookAt(XMFLOAT3(0.0f, 0.0f, 0.0f));

    GameObject* terrainObject = scene->CreateGameObject("Terrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(64, 64, 2.0f);   // 64 x 64 칸, 칸 한 변 2 → 128 x 128 크기

    switch (mode)
    {
    case TerrainMode::Flat:
        terrain->SetDisplayMode(TerrainRenderer::DisplayMode::FlatGrid);
        break;

    case TerrainMode::Noise:
        terrain->SetHeightSourceNoise();
        terrain->SetDisplayMode(TerrainRenderer::DisplayMode::HeightColor);
        break;

    case TerrainMode::Image:
        // 이미지 값은 0~1 이라 노이즈보다 진폭을 크게 줘야 굴곡이 보인다.
        terrain->SetHeightSourceImages({ L"Assets/heightmap.png",
                                         L"Assets/heightmap2.png",
                                         L"Assets/heightmap3.png" }, 26.0f);
        terrain->SetDisplayMode(TerrainRenderer::DisplayMode::HeightColor);
        break;

    case TerrainMode::Splatting:
        // 스플래팅은 경사도가 있어야 바위 레이어가 드러난다.
        // 완만한 지형이면 절벽이 없어 바위가 한 픽셀도 안 나온다.
        terrain->SetHeightSourceNoise(/*amplitude*/ 34.0f, /*frequency*/ 0.022f);
        terrain->SetDisplayMode(TerrainRenderer::DisplayMode::Splatting);
        break;
    }
}

// -------------------------------------------------------------
// 청크 기반 터레인 씬 (스텝 5 : 컬링, 스텝 6 : LOD)
// -------------------------------------------------------------
void Game::BuildChunkedTerrainScene(ChunkedMode mode, const std::string& sceneName)
{
    Scene* scene = SceneManager::Get().CreateScene(sceneName);
    if (!scene)
        return;

    GameObject* cameraObject = scene->CreateGameObject("MainCamera");
    Camera* camera = cameraObject->AddComponent<Camera>();

    if (mode == ChunkedMode::Sky || mode == ChunkedMode::Clouds || mode == ChunkedMode::Infinite || mode == ChunkedMode::Biomes || mode == ChunkedMode::Water)
    {
        // 지평선과 하늘이 함께 보이도록 낮게 서서 앞을 본다.
        camera->SetPosition(XMFLOAT3(0.0f, 70.0f, -260.0f));
        camera->SetAngles(0.0f, -6.0f);
    }
    else
    {
        camera->SetPosition(XMFLOAT3(0.0f, 90.0f, -150.0f));
        camera->LookAt(XMFLOAT3(0.0f, 0.0f, 0.0f));
    }

    GameObject* terrainObject = scene->CreateGameObject("Terrain");
    ChunkedTerrainRenderer* terrain = terrainObject->AddComponent<ChunkedTerrainRenderer>();

    // 16 x 16 청크 x 한 청크 16칸 x 칸 4 = 1024 x 1024 크기, 청크 256개
    terrain->SetGrid(16, 16, 16, 4.0f);

    terrain::HeightParams height;
    height.amplitude = 60.0f;
    height.frequency = 0.006f;

    if (mode == ChunkedMode::Biomes)
    {
        // 바이옴 하나하나가 수백 단위로 넓으므로 더 멀리까지 보이게 청크를 늘린다.
        // 24 x 24 청크 = 1536 x 1536. 작업 스레드가 있어 스트리밍은 버틴다.
        terrain->SetGrid(24, 24, 16, 4.0f);
        height.amplitude = 85.0f;
        height.frequency = 0.005f;
        height.biomes = true;
        height.biomeFrequency = 0.0022f;
    }

    if (mode == ChunkedMode::Water)
    {
        // 사막과 낮은 초원이 수면 아래로 잠겨 호수와 바다가 된다.
        height.amplitude = 85.0f;
        height.frequency = 0.005f;
        height.biomes = true;
        height.biomeFrequency = 0.0022f;
    }

    terrain->SetHeightParams(height);

    // 하늘은 지형보다 먼저 그려야 하므로 씬에 먼저 넣는다.
    //  (Scene 은 소유 순서대로 Render 한다)
    SkyRenderer* sky = nullptr;
    if (mode == ChunkedMode::Sky || mode == ChunkedMode::Clouds || mode == ChunkedMode::Infinite || mode == ChunkedMode::Biomes || mode == ChunkedMode::Water)
    {
        GameObject* skyObject = scene->CreateGameObject("Sky");
        sky = skyObject->AddComponent<SkyRenderer>();
        sky->SetCloudsEnabled(mode != ChunkedMode::Sky);

        // 대기 원근 (S64)
        //  유한 지형 : 멀리까지 옅게 깔아 거리감만 준다.
        //  무한 지형 : 스트리밍 반경 안에서 반드시 완전히 덮어, 아직 없는 청크를 가린다.
        if (mode == ChunkedMode::Infinite)
            sky->SetFogRange(60.0f, terrain->GetStreamingRadius() - 32.0f, 0.0028f);
        else if (mode == ChunkedMode::Biomes)
            sky->SetFogRange(250.0f, terrain->GetStreamingRadius() - 32.0f, 0.0007f);
        else if (mode == ChunkedMode::Water)
            sky->SetFogRange(150.0f, terrain->GetStreamingRadius() - 32.0f, 0.0012f);
        else
            sky->SetFogRange(150.0f, 950.0f, 0.0016f);
    }

    terrain->SetCullingEnabled(true);

    switch (mode)
    {
    case ChunkedMode::Culling:
        terrain->SetLodEnabled(false);
        terrain->SetSkirtEnabled(false);
        terrain->SetDisplayMode(ChunkedTerrainRenderer::DisplayMode::ChunkColor);
        break;

    case ChunkedMode::Lod:
        // 스텝 6 : LOD 만. 스커트와 모핑을 꺼서 균열과 팝핑이 그대로 보이게 한다.
        terrain->SetLodEnabled(true);
        terrain->SetSkirtEnabled(false);
        terrain->SetMorphEnabled(false);
        terrain->SetDisplayMode(ChunkedTerrainRenderer::DisplayMode::LodColor);
        break;

    case ChunkedMode::LodAdvanced:
        // 스텝 6-2 : 스커트로 균열을 메우고 지오모핑으로 팝핑을 없앤다.
        terrain->SetLodEnabled(true);
        terrain->SetSkirtEnabled(true);
        terrain->SetMorphEnabled(true);
        terrain->SetDisplayMode(ChunkedTerrainRenderer::DisplayMode::LodColor);
        break;

    case ChunkedMode::Sky:
    case ChunkedMode::Clouds:
        // 스텝 8~9 : 하늘이 주인공이므로 지형은 텍스처를 입힌 상태로 둔다.
        terrain->SetLodEnabled(true);
        terrain->SetSkirtEnabled(true);
        terrain->SetMorphEnabled(true);
        terrain->SetDisplayMode(ChunkedTerrainRenderer::DisplayMode::Splatting);
        break;

    case ChunkedMode::Infinite:
        // 스텝 10 : 청크를 재활용해 끝없이 이어지는 지형.
        //  이미지 하이트맵은 범위가 정해져 있어 쓸 수 없다. 노이즈여야 한다.
        terrain->SetLodEnabled(true);
        terrain->SetSkirtEnabled(true);
        terrain->SetMorphEnabled(true);
        terrain->SetInfiniteEnabled(true);
        terrain->SetDisplayMode(ChunkedTerrainRenderer::DisplayMode::Splatting);
        break;

    case ChunkedMode::Biomes:
        // 에디터 연결 : 바이옴 (S73). 무한 지형 위에 기후를 얹는다.
        terrain->SetLodEnabled(true);
        terrain->SetSkirtEnabled(true);
        terrain->SetMorphEnabled(true);
        terrain->SetInfiniteEnabled(true);
        terrain->SetDisplayMode(ChunkedTerrainRenderer::DisplayMode::Splatting);
        camera->SetPosition(XMFLOAT3(0.0f, 280.0f, -420.0f));
        camera->SetAngles(0.0f, -24.0f);
        break;

    case ChunkedMode::Water:
    {
        // 눈에 띄는 작업 : 물 표면 (S76~S78)
        terrain->SetLodEnabled(true);
        terrain->SetSkirtEnabled(true);
        terrain->SetMorphEnabled(true);
        terrain->SetInfiniteEnabled(true);
        terrain->SetDisplayMode(ChunkedTerrainRenderer::DisplayMode::Splatting);

        // 수면 가까이 낮게 서서 멀리 본다. 비스듬히 볼수록 반사가 강해진다(프레넬).
        camera->SetPosition(XMFLOAT3(0.0f, 30.0f, -140.0f));
        camera->SetAngles(20.0f, -3.0f);

        GameObject* waterObject = scene->CreateGameObject("Water");
        WaterRenderer* water = waterObject->AddComponent<WaterRenderer>();
        water->SetWaterLevel(6.0f);
        break;
    }
    }
}

// -------------------------------------------------------------
// 스텝 7 : 하드웨어 테셀레이션
// -------------------------------------------------------------
void Game::BuildTessellationScene()
{
    Scene* scene = SceneManager::Get().CreateScene("Terrain_Step7");
    if (!scene)
        return;

    GameObject* cameraObject = scene->CreateGameObject("MainCamera");
    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetPosition(XMFLOAT3(0.0f, 120.0f, -320.0f));
    camera->SetAngles(0.0f, -14.0f);

    GameObject* skyObject = scene->CreateGameObject("Sky");
    SkyRenderer* sky = skyObject->AddComponent<SkyRenderer>();
    sky->SetFogRange(150.0f, 950.0f, 0.0016f);

    GameObject* terrainObject = scene->CreateGameObject("Terrain");
    TessellatedTerrainRenderer* terrain = terrainObject->AddComponent<TessellatedTerrainRenderer>();

    // 제어점은 아주 성기다. 32 x 32 패치 = 제어점 1089개뿐이다.
    terrain->SetGrid(32, 32, 32.0f);

    terrain::HeightParams height;
    height.amplitude = 70.0f;
    height.frequency = 0.0035f;
    terrain->SetHeightParams(height);

    // 처음에는 분할 패턴이 눈에 보이도록 낮게. + 키로 올려 가며 비교한다.
    terrain->SetTessellationRange(1.0f, 10.0f);
    terrain->SetWireframe(true);   // 분할이 눈에 보이도록 처음엔 와이어프레임
}

// -------------------------------------------------------------
// 에디터 : 지형 브러시 (S67~S70)
//  청크 지형을 편집 가능한 격자로 굽고, 같은 오브젝트에 브러시를 붙인다.
// -------------------------------------------------------------
void Game::BuildTerrainEditorScene(bool startErosion)
{
    Scene* scene = SceneManager::Get().CreateScene(startErosion ? "Terrain_Erosion" : "Terrain_Editor");
    if (!scene)
        return;

    GameObject* cameraObject = scene->CreateGameObject("MainCamera");
    Camera* camera = cameraObject->AddComponent<Camera>();
    if (startErosion)
    {
        // 물길은 가까이서 봐야 보인다.
        camera->SetPosition(XMFLOAT3(-40.0f, 95.0f, -170.0f));
        camera->LookAt(XMFLOAT3(0.0f, 0.0f, 10.0f));
    }
    else
    {
        camera->SetPosition(XMFLOAT3(0.0f, 120.0f, -250.0f));
        camera->LookAt(XMFLOAT3(0.0f, 0.0f, 20.0f));
    }

    GameObject* skyObject = scene->CreateGameObject("Sky");
    SkyRenderer* sky = skyObject->AddComponent<SkyRenderer>();
    sky->SetFogRange(250.0f, 950.0f, 0.0010f);

    GameObject* terrainObject = scene->CreateGameObject("Terrain");
    ChunkedTerrainRenderer* terrain = terrainObject->AddComponent<ChunkedTerrainRenderer>();

    // 16 x 16 청크 x 16칸 x 칸 2 = 512 x 512, 편집 격자 257 x 257
    terrain->SetGrid(16, 16, 16, 2.0f);

    terrain::HeightParams height;
    height.amplitude = startErosion ? 55.0f : 40.0f;   // 침식은 가파른 산일수록 물길이 뚜렷하다
    height.frequency = 0.008f;
    terrain->SetHeightParams(height);

    terrain->SetCullingEnabled(true);
    terrain->SetLodEnabled(true);
    terrain->SetSkirtEnabled(true);
    terrain->SetMorphEnabled(true);
    terrain->SetDisplayMode(ChunkedTerrainRenderer::DisplayMode::Splatting);

    // 함수를 표본으로 굽는다. 이제부터 높이는 격자 값이다.
    terrain->EnableEditing();

    terrainObject->AddComponent<TerrainBrush>();

    // 수력 침식 (S74). R 로 켜고 끈다.
    TerrainErosion* erosion = terrainObject->AddComponent<TerrainErosion>();
    erosion->SetRunning(startErosion);
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

// -------------------------------------------------------------
// 씬 그리기 (S76, S77)
//  물이 있으면 수면에 비칠 세상을 먼저 한 번 더 그린다.
//  그다음 불투명한 것을 모두 그리고, 마지막에 물처럼 그 결과를 읽는 것을 그린다.
// -------------------------------------------------------------
void Game::RenderScenePasses()
{
    Scene* scene = SceneManager::Get().GetActiveScene();
    if (!scene || !m_graphics)
        return;

    WaterRenderer* water = nullptr;
    for (const auto& object : scene->GetGameObjects())
    {
        if (!object || object->IsPendingDestroy() || !object->IsActive())
            continue;

        water = object->GetComponent<WaterRenderer>();
        if (water && water->IsEnabled())
            break;
        water = nullptr;
    }

    if (water && water->IsReflectionEnabled() && m_graphics->BeginReflectionPass(water->GetWaterLevel()))
    {
        scene->RenderOpaque();
        m_graphics->EndReflectionPass();
    }

    scene->RenderOpaque();
    scene->RenderTransparent();
}

int Game::Run()
{
    if (!m_running)
        return -1;

    TimeManager& time = TimeManager::Get();
    InputManager& input = InputManager::Get();
    SceneManager& sceneManager = SceneManager::Get();

    // ---- 게임 루프 ----
    //  메뉴도 하나의 씬이라 분기가 없다. 항상 같은 순서로 돈다.
    while (m_window->ProcessMessages())        // 1. 메시지 처리
    {
        time.Update();
        input.Update();                        // 2. 입력 갱신

        UpdateEditorUI();                      //    Hierarchy / Inspector (쇼케이스에서만)
        UpdatePickingAndDrag();                //    피킹 / 드래그 (쇼케이스에서만)

#if HOT_RELOAD_ENABLED
        ShaderManager::Get().Update(time.GetDeltaTime());
#endif

        sceneManager.Update();                 // 3. 씬 Update (메뉴도 여기서 돈다)

        m_graphics->BeginFrame();              // 4. 화면 Clear
        RenderScenePasses();                   // 5. 씬 Render (반사 패스 → 불투명 → 반투명)
        DrawOverlayUI();                       //    메뉴 / 에디터 패널 오버레이
        m_graphics->EndFrame();                // 6. Present

        sceneManager.ProcessPendingChanges();  // 7. 추가/삭제 일괄 반영
        HandleFrameEndCommands();              // 8. 저장/로드 등
        ProcessPendingSceneChange();           // 9. 씬 전환은 가장 마지막에
        UpdateWindowTitle();
    }

    return 0;
}

// -------------------------------------------------------------
// 메뉴에 올릴 목록.
//  스텝이 늘어나면 여기에 push_back 한 줄만 더하면 된다.
// -------------------------------------------------------------
void Game::SetupShowcaseList()
{
    m_showcases.clear();

    m_showcases.push_back({
        L"터레인 · 스텝 1  평면 그리드",
        L"격자 메시 생성 · 원근 카메라 · 깊이 버퍼 (S27~S35)",
        [this]() { BuildTerrainScene(TerrainMode::Flat, "Terrain_Step1"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 2  펄린 노이즈 지형",
        L"펄린 노이즈 fBm · 중앙 차분 법선 · 램버트 조명 (S36~S40)",
        [this]() { BuildTerrainScene(TerrainMode::Noise, "Terrain_Step2"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 3  높이맵 이미지 지형",
        L"회색조 이미지 → 높이 · 이중선형 보간 · 8비트 양자화 (S41~S42)",
        [this]() { BuildTerrainScene(TerrainMode::Image, "Terrain_Step3"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 4  텍스처 스플래팅",
        L"경사도 / 높이 기반 4레이어 블렌딩 · UV 타일링 (S44~S47)",
        [this]() { BuildTerrainScene(TerrainMode::Splatting, "Terrain_Step4"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 5  쿼드트리 컬링",
        L"청크 분할 · 절두체 평면 6장 · 쿼드트리로 통째 버리기 (S48~S50)",
        [this]() { BuildChunkedTerrainScene(ChunkedMode::Culling, "Terrain_Step5"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 6  거리 기반 LOD",
        L"거리에 따라 인덱스 간격을 벌려 삼각형 줄이기 (S51)",
        [this]() { BuildChunkedTerrainScene(ChunkedMode::Lod, "Terrain_Step6"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 6-2  스티칭 & 지오모핑",
        L"스커트로 LOD 경계 균열 메우기 · 모프 타깃으로 팝핑 제거 (S52~S53)",
        [this]() { BuildChunkedTerrainScene(ChunkedMode::LodAdvanced, "Terrain_Step6b"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 7  하드웨어 테셀레이션",
        L"성긴 패치를 GPU 가 실시간 분할 · Hull/Domain 셰이더 (S59~S62)",
        [this]() { BuildTessellationScene(); } });

    m_showcases.push_back({
        L"터레인 · 스텝 8  스카이맵 (SkyDome)",
        L"돔을 카메라에 붙이고 방향으로 하늘색 계산 · 태양 (S54~S55)",
        [this]() { BuildChunkedTerrainScene(ChunkedMode::Sky, "Terrain_Step8"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 9  동적 왜곡 구름",
        L"노이즈로 노이즈를 미는 도메인 워핑 · 시간에 따라 흐른다 (S56~S57)",
        [this]() { BuildChunkedTerrainScene(ChunkedMode::Clouds, "Terrain_Step9"); } });

    m_showcases.push_back({
        L"터레인 · 스텝 10  무한 지형 청크",
        L"카메라를 따라 청크를 재활용 · 프레임당 재생성 개수 제한 (S58)",
        [this]() { BuildChunkedTerrainScene(ChunkedMode::Infinite, "Terrain_Step10"); } });

    m_showcases.push_back({
        L"터레인 에디터 · 지형 브러시",
        L"마우스 레이 피킹 · 올리기/내리기/평탄화/부드럽게 · 바뀐 청크만 재생성 · 저장 (S67~S70)",
        [this]() { BuildTerrainEditorScene(); } });

    m_showcases.push_back({
        L"터레인 에디터 연결 · 바이옴",
        L"온도 · 습도 노이즈 → 바이옴 가중치 · 바이옴마다 다른 높이 식 · 정점 속성으로 넘기기 (S73)",
        [this]() { BuildChunkedTerrainScene(ChunkedMode::Biomes, "Terrain_Biomes"); } });

    m_showcases.push_back({
        L"터레인 · 수력 침식",
        L"물방울 입자가 흙을 깎고 쌓아 계곡을 만든다 · 시간 예산 · 대량 재생성은 작업 스레드로 (S74)",
        [this]() { BuildTerrainEditorScene(true); } });

    m_showcases.push_back({
        L"터레인 · 물 표면",
        L"반사 패스 · 깊이 복사 · 프레넬 · 컬 노이즈 흐름 · 클릭하면 퍼지는 물결 (S76~S80)",
        [this]() { BuildChunkedTerrainScene(ChunkedMode::Water, "Terrain_Water"); } });

    m_showcases.push_back({
        L"2D 스프라이트 데모",
        L"계층 Transform · 피킹 · 드래그 · Hierarchy / Inspector",
        [this]() { BuildSpriteDemoScene(); } });
}

// -------------------------------------------------------------
// 메인 메뉴 씬.
//  Scene 을 상속하지 않는다. 평범한 씬에 MenuController 를 붙일 뿐이다.
// -------------------------------------------------------------
void Game::BuildMenuScene()
{
    Scene* scene = SceneManager::Get().CreateScene("MainMenu");
    if (!scene)
        return;

    GameObject* root = scene->CreateGameObject("MenuRoot");
    MenuController* menu = root->AddComponent<MenuController>();

    std::vector<MenuScreen::Entry> entries;
    entries.reserve(m_showcases.size() + 1);

    for (const Showcase& showcase : m_showcases)
        entries.push_back({ showcase.title, showcase.description });

    entries.push_back({ L"종료", L"프로그램을 끝낸다" });

    menu->SetEntries(std::move(entries));

    // 고른 순간에는 요청만 남긴다. 실제 전환은 프레임 끝에.
    menu->SetOnSelect([this](int index) { RequestScene(index); });

    m_currentShowcase = kMenuScene;
    m_showEditorPanels = false;
    m_selectedId = 0;
    m_dragging = false;
}

void Game::RequestScene(int index)
{
    m_pendingScene = index;
}

// -------------------------------------------------------------
// 씬 전환은 Update / Render 가 모두 끝난 뒤에만 한다.
//  순회 중에 씬을 파괴하면 그 씬의 컴포넌트가 자기 발밑을 무너뜨린다.
// -------------------------------------------------------------
void Game::ProcessPendingSceneChange()
{
    if (m_pendingScene == kNoRequest)
        return;

    const int request = m_pendingScene;
    m_pendingScene = kNoRequest;

    if (request == kMenuScene)
    {
        BuildMenuScene();
        SceneManager::Get().Initialize(m_graphics.get());
        dxutil::DebugLog(L"[Game] 메뉴 씬으로");
        return;
    }

    // 목록 범위를 넘어가면 마지막 항목(종료)이다.
    if (request < 0 || request >= static_cast<int>(m_showcases.size()))
    {
        ::PostQuitMessage(0);
        return;
    }

    m_selectedId = 0;
    m_dragging = false;
    m_currentShowcase = request;
    m_showEditorPanels = true;

    m_showcases[request].build();
    SceneManager::Get().Initialize(m_graphics.get());

    dxutil::DebugLog(L"[Game] 쇼케이스 진입 : %s (ESC 로 메뉴 복귀)",
                     m_showcases[request].title.c_str());
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
    if (!m_showEditorPanels)
    {
        InputManager::Get().SetPointerOverUI(false);
        return;    // 메뉴 씬에서는 패널을 띄우지 않는다
    }

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

    // 패널 위의 마우스는 UI 몫이다. 지형 브러시 같은 게임 쪽 입력은 이 값을 보고 물러난다.
    input.SetPointerOverUI(m_hierarchy.Contains(input.GetMouseX(), input.GetMouseY()) ||
                           m_inspector.Contains(input.GetMouseX(), input.GetMouseY()));

    UpdateControlsPanel();
}

// -------------------------------------------------------------
// 조작 안내
//  현재 씬에 있는 것만 보여준다. 지금 상태(표시 모드 등)도 함께 띄운다.
// -------------------------------------------------------------
void Game::UpdateControlsPanel()
{
    Scene* scene = SceneManager::Get().GetActiveScene();
    if (!scene)
        return;

    TerrainRenderer* terrain = nullptr;
    for (const auto& object : scene->GetGameObjects())
    {
        if (!object || object->IsPendingDestroy())
            continue;
        if (TerrainRenderer* found = object->GetComponent<TerrainRenderer>())
        {
            terrain = found;
            break;
        }
    }

    ChunkedTerrainRenderer* chunked = nullptr;
    for (const auto& object : scene->GetGameObjects())
    {
        if (!object || object->IsPendingDestroy())
            continue;
        if (ChunkedTerrainRenderer* found = object->GetComponent<ChunkedTerrainRenderer>())
        {
            chunked = found;
            break;
        }
    }

    TessellatedTerrainRenderer* tess = nullptr;
    for (const auto& object : scene->GetGameObjects())
    {
        if (!object || object->IsPendingDestroy())
            continue;
        if (TessellatedTerrainRenderer* found = object->GetComponent<TessellatedTerrainRenderer>())
        {
            tess = found;
            break;
        }
    }

    SkyRenderer* sky = nullptr;
    Camera* camera = nullptr;
    WaterRenderer* water = nullptr;
    for (const auto& object : scene->GetGameObjects())
    {
        if (!object || object->IsPendingDestroy())
            continue;
        if (!sky)    sky = object->GetComponent<SkyRenderer>();
        if (!camera) camera = object->GetComponent<Camera>();
        if (!water)  water = object->GetComponent<WaterRenderer>();
    }

    auto onOff = [](bool on) { return on ? L"켬" : L"끔"; };

    std::vector<ControlsPanel::Line> lines;

    if (tess)
    {
        wchar_t factor[48];
        _snwprintf_s(factor, _countof(factor), _TRUNCATE, L"%.0f ~ %.0f",
                     tess->GetMinFactor(), tess->GetMaxFactor());

        wchar_t patches[48];
        _snwprintf_s(patches, _countof(patches), _TRUNCATE, L"%d개", tess->GetPatchCount());

        m_controls.SetTitle(L"조작   (Tab 으로 와이어프레임)");
        lines.push_back({ L"Tab", L"와이어프레임", onOff(tess->IsWireframe()), true });
        lines.push_back({ L"+ / -", L"최대 분할", factor, true });
        lines.push_back({ L"N", L"새 지형 생성", L"", false });
        lines.push_back({ L"", L"보내는 패치", patches, true });

        // 에디터 연결 : 컴퓨트 셰이더 (S71, S72)
        wchar_t sizeText[48];
        _snwprintf_s(sizeText, _countof(sizeText), _TRUNCATE, L"%d x %d", tess->GetHeightMapSize(), tess->GetHeightMapSize());

        wchar_t cpuText[48];
        if (tess->GetLastCpuMs() >= 0.0f)
            _snwprintf_s(cpuText, _countof(cpuText), _TRUNCATE, L"%.1f ms", tess->GetLastCpuMs());
        else
            _snwprintf_s(cpuText, _countof(cpuText), _TRUNCATE, L"-");

        wchar_t gpuText[64];
        if (tess->GetLastGpuMs() >= 0.0f)
            _snwprintf_s(gpuText, _countof(gpuText), _TRUNCATE, L"%.1f ms · 읽어 오기 %.1f ms",
                         tess->GetLastGpuMs(), tess->GetLastReadbackMs());
        else
            _snwprintf_s(gpuText, _countof(gpuText), _TRUNCATE, L"-");

        wchar_t rangeText[64];
        _snwprintf_s(rangeText, _countof(rangeText), _TRUNCATE, L"%.2f ~ %.2f", tess->GetMinHeight(), tess->GetMaxHeight());

        const wchar_t* generator = tess->IsGpuGeneration()
            ? (tess->IsGpuGenerationAvailable() ? L"GPU (컴퓨트 셰이더)" : L"GPU 불가 → CPU")
            : L"CPU";

        lines.push_back({ L"U", L"높이맵 생성", generator, true });
        lines.push_back({ L"R", L"높이맵 해상도", sizeText, true });
        lines.push_back({ L"", L"CPU 생성", cpuText, true });
        lines.push_back({ L"", L"GPU 생성", gpuText, true });
        lines.push_back({ L"", L"높이 범위", rangeText, true });
    }
    else if (chunked)
    {
        const terrain::TerrainQuadTree::Stats& stats = chunked->GetStats();

        wchar_t drawing[96];
        _snwprintf_s(drawing, _countof(drawing), _TRUNCATE, L"%d / %d 청크 · %d 삼각형",
                     stats.visibleChunks, stats.totalChunks, chunked->GetDrawnTriangles());

        m_controls.SetTitle(L"조작   (Tab 으로 보기 전환)");
        lines.push_back({ L"Tab", L"표시 모드", chunked->GetDisplayModeName(), true });
        lines.push_back({ L"T", L"트라이플래너", onOff(chunked->IsTriplanarEnabled()), true });
        lines.push_back({ L"C", L"절두체 컬링", onOff(chunked->IsCullingEnabled()), true });
        lines.push_back({ L"L", L"거리 LOD", onOff(chunked->IsLodEnabled()), true });
        lines.push_back({ L"K", L"스커트", onOff(chunked->IsSkirtEnabled()), true });
        lines.push_back({ L"M", L"지오모핑", onOff(chunked->IsMorphEnabled()), true });
        lines.push_back({ L"N", L"새 지형 생성", L"", false });

        if (!chunked->IsEditable())
            lines.push_back({ L"Y", L"바이옴", onOff(chunked->IsBiomesEnabled()), true });

        if (chunked->IsInfiniteEnabled())
        {
            wchar_t worker[64];
            if (chunked->IsAsyncBuildEnabled())
                _snwprintf_s(worker, _countof(worker), _TRUNCATE, L"켬 · 스레드 %d개", chunked->GetWorkerThreadCount());
            else
                _snwprintf_s(worker, _countof(worker), _TRUNCATE, L"끔 · 메인 스레드");

            wchar_t cost[96];
            _snwprintf_s(cost, _countof(cost), _TRUNCATE, L"%.2f ms · 대기 %zu개",
                         chunked->GetMainThreadBuildMs(), chunked->GetOutstandingBuildCount());

            lines.push_back({ L"J", L"무한 지형", L"켬", true });
            lines.push_back({ L"B", L"백그라운드 생성", worker, true });
            lines.push_back({ L"", L"메인 스레드 비용", cost, true });
        }

        // 편집 가능한 지형이면 브러시 조작을 보여 준다.
        if (TerrainBrush* brush = chunked->GetOwner()->GetComponent<TerrainBrush>())
        {
            wchar_t radius[32];
            _snwprintf_s(radius, _countof(radius), _TRUNCATE, L"%.0f", brush->GetRadius());

            wchar_t strength[32];
            _snwprintf_s(strength, _countof(strength), _TRUNCATE, L"%.1f", brush->GetStrength());

            wchar_t hit[64];
            if (brush->HasHit())
                _snwprintf_s(hit, _countof(hit), _TRUNCATE, L"(%.0f, %.1f, %.0f)",
                             brush->GetHitPoint().x, brush->GetHitPoint().y, brush->GetHitPoint().z);
            else
                _snwprintf_s(hit, _countof(hit), _TRUNCATE, L"-");

            wchar_t rebuilt[64];
            if (chunked->GetLastEditRebuildMs() < 0.0f)
                _snwprintf_s(rebuilt, _countof(rebuilt), _TRUNCATE, L"%d개 · 작업 스레드",
                             chunked->GetLastEditRebuildCount());
            else
                _snwprintf_s(rebuilt, _countof(rebuilt), _TRUNCATE, L"%d개 · %.2f ms",
                             chunked->GetLastEditRebuildCount(), chunked->GetLastEditRebuildMs());

            lines.push_back({ L"좌클릭 드래그", L"지형 칠하기", L"", false });
            lines.push_back({ L"1 ~ 5", L"브러시 도구", brush->GetToolName(), true });
            lines.push_back({ L"휠", L"브러시 반경", radius, true });
            lines.push_back({ L"[ / ]", L"브러시 세기", strength, true });
            lines.push_back({ L"", L"가리킨 곳", hit, true });
            lines.push_back({ L"", L"다시 만든 청크", rebuilt, true });

            if (TerrainErosion* erosion = chunked->GetOwner()->GetComponent<TerrainErosion>())
            {
                wchar_t droplets[96];
                _snwprintf_s(droplets, _countof(droplets), _TRUNCATE, L"이번 실행 %d / %d · 이번 프레임 %d개",
                             erosion->GetRunDroplets(), TerrainErosion::kDropletsPerRun, erosion->GetDropletsLastFrame());

                wchar_t cost[48];
                _snwprintf_s(cost, _countof(cost), _TRUNCATE, L"%.1f ms", erosion->GetLastFrameMs());

                lines.push_back({ L"R", L"수력 침식 한 번 더", erosion->IsRunning() ? L"실행 중" : L"멈춤", true });
                lines.push_back({ L"", L"떨어뜨린 물방울", droplets, true });
                lines.push_back({ L"", L"침식 계산", cost, true });
            }
        }

        lines.push_back({ L"", L"그리는 중", drawing, true });
    }
    else if (terrain)
    {
        m_controls.SetTitle(L"조작   (Tab 으로 보기 전환)");
        lines.push_back({ L"Tab", L"표시 모드", terrain->GetDisplayModeName(), true });

        if (terrain->GetDisplayMode() == TerrainRenderer::DisplayMode::Splatting)
            lines.push_back({ L"T", L"트라이플래너", onOff(terrain->IsTriplanarEnabled()), true });

        // 이미지 높이맵에서는 seed 를 바꿔도 화면이 그대로다.
        // 그래서 N 은 "다음 높이맵" 으로, P(노이즈 종류)는 아예 숨긴다.
        if (terrain->GetHeightSource() == terrain::HeightSource::Image)
        {
            lines.push_back({ L"N", L"다음 높이맵", terrain->GetCurrentImageName(), true });
        }
        else
        {
            lines.push_back({ L"N", L"새 지형 생성", L"", false });
            lines.push_back({ L"P", L"노이즈 종류",
                              terrain->GetNoiseType() == terrain::NoiseType::Perlin ? L"펄린" : L"값(value)", false });
        }
    }
    else
    {
        m_controls.SetTitle(L"조작");
        lines.push_back({ L"좌클릭", L"오브젝트 선택", L"", false });
        lines.push_back({ L"드래그", L"오브젝트 이동", L"", false });
        lines.push_back({ L"F1 / F2", L"자식 추가 / 삭제", L"", false });
    }

    // 하늘이 있는 씬 : 구름과 안개
    if (sky)
    {
        // 테셀레이션 씬은 + / - 를 분할 계수에 쓰므로 구름 조작은 보여 주지 않는다.
        if (!tess)
        {
            wchar_t coverage[48];
            _snwprintf_s(coverage, _countof(coverage), _TRUNCATE, L"%.0f%%", sky->GetCloudCoverage() * 100.0f);

            lines.push_back({ L"V", L"구름", onOff(sky->AreCloudsEnabled()), true });
            lines.push_back({ L"+ / -", L"구름 양", coverage, true });
        }
        lines.push_back({ L"O", L"안개", onOff(sky->IsFogEnabled()), true });
    }

    // 물이 있는 씬 (S76~S78)
    if (water)
    {
        wchar_t level[32];
        _snwprintf_s(level, _countof(level), _TRUNCATE, L"%.1f", water->GetWaterLevel());

        lines.push_back({ L"[ / ]", L"수위", level, true });
        lines.push_back({ L"X", L"반사 패스", onOff(water->IsReflectionEnabled()), true });
        lines.push_back({ L"Z", L"굴절 · 깊이", onOff(water->IsRefractionEnabled()), true });
        lines.push_back({ L"U", L"물 흐름 (컬 노이즈)", onOff(water->IsFlowEnabled()), true });
        lines.push_back({ L"R", L"빗방울", onOff(water->IsRainEnabled()), true });
        lines.push_back({ L"좌클릭 / 드래그", L"물결 일으키기", L"", false });
        lines.push_back({ L"1", L"물결 높이 보기", onOff(water->IsRippleDebugEnabled()), true });
        lines.push_back({ L"", L"해안 반사", water->IsBuildingShore() ? L"굽는 중" : (water->HasShore() ? L"켬" : L"-"), true });
    }

    // 카메라가 있는 씬 : 이동 방식
    if (camera)
    {
        lines.push_back({ L"G", L"이동 방식", camera->IsWalking() ? L"걷기" : L"비행", true });
        if (camera->IsWalking())
            lines.push_back({ L"우클릭+Space", L"점프", L"", false });
        lines.push_back({ L"우클릭+WASD", L"카메라 이동 · 회전", L"", false });
        lines.push_back({ L"F", L"카메라 리셋", L"", false });
    }

    lines.push_back({ L"H / I", L"패널 켜기/끄기", L"", false });
    lines.push_back({ L"F5 / F9", L"씬 저장 / 불러오기", L"", false });
    lines.push_back({ L"ESC", L"메뉴로 돌아가기", L"", false });

    m_controls.SetLines(std::move(lines));
}

void Game::DrawOverlayUI()
{
    if (!m_graphics)
        return;

    // 오버레이 DC 는 프레임당 한 번만 잡고 모두가 나눠 쓴다.
    HDC hdc = m_graphics->BeginOverlay();
    if (!hdc)
        return;

    // 씬 안의 UI 컴포넌트(메뉴)를 먼저 그린다.
    if (Scene* scene = SceneManager::Get().GetActiveScene())
    {
        for (const auto& object : scene->GetGameObjects())
        {
            if (!object || object->IsPendingDestroy())
                continue;

            if (MenuController* menu = object->GetComponent<MenuController>())
                menu->DrawOverlay(hdc);
        }
    }

    if (m_showEditorPanels)
    {
        m_hierarchy.Draw(hdc, m_selectedId);
        m_inspector.Draw(hdc);
        m_controls.Draw(hdc, m_graphics->GetWidth(), m_graphics->GetHeight());
    }

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
    if (!m_showEditorPanels)
        return;    // 메뉴 씬에는 고를 스프라이트가 없다

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

    // 우클릭은 카메라 프리룩에 쓰므로 선택 해제로 쓰지 않는다.
    // 선택을 풀려면 빈 곳을 좌클릭하면 된다.

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

    // ESC : 쇼케이스면 메뉴로, 메뉴에서 누르면 종료
    if (input.GetKeyDown(VK_ESCAPE) && !m_inspector.IsEditing())
    {
        if (m_currentShowcase == kMenuScene)
            ::PostQuitMessage(0);
        else
            RequestScene(kMenuScene);
        return;
    }

    if (!m_showEditorPanels)
        return;    // 아래는 쇼케이스 전용

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

        // ---- 청크 지형 (스텝 5 이후) ----
        //  주의 : TerrainRenderer 를 먼저 걸러 내면 여기까지 오지 못한다.
        //         청크 씬에는 TerrainRenderer 가 없기 때문이다.
        if (ChunkedTerrainRenderer* chunked = object->GetComponent<ChunkedTerrainRenderer>())
        {
            if (input.GetKeyDown(VK_TAB))
            {
                chunked->CycleDisplayMode();
                dxutil::DebugLog(L"[Terrain] 표시 모드 : %s", chunked->GetDisplayModeName());
            }
            if (input.GetKeyDown('C'))
            {
                chunked->ToggleCulling();
                dxutil::DebugLog(L"[Terrain] 절두체 컬링 %s", chunked->IsCullingEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('L'))
            {
                chunked->ToggleLod();
                dxutil::DebugLog(L"[Terrain] 거리 LOD %s", chunked->IsLodEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('K'))
            {
                chunked->ToggleSkirt();
                dxutil::DebugLog(L"[Terrain] 스커트 %s", chunked->IsSkirtEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('M'))
            {
                chunked->ToggleMorph();
                dxutil::DebugLog(L"[Terrain] 지오모핑 %s", chunked->IsMorphEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('N'))
                chunked->Regenerate(static_cast<unsigned>(TimeManager::Get().GetFrameCount() * 2654435761u + 7u));
            if (input.GetKeyDown('J'))
            {
                chunked->ToggleInfinite();
                dxutil::DebugLog(L"[Terrain] 무한 지형 %s", chunked->IsInfiniteEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('T'))
            {
                chunked->ToggleTriplanar();
                dxutil::DebugLog(L"[Terrain] 트라이플래너 %s", chunked->IsTriplanarEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('B'))
            {
                chunked->ToggleAsyncBuild();
                dxutil::DebugLog(L"[Terrain] 백그라운드 생성 %s", chunked->IsAsyncBuildEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('Y'))
            {
                chunked->ToggleBiomes();
                dxutil::DebugLog(L"[Terrain] 바이옴 %s", chunked->IsBiomesEnabled() ? L"켬" : L"끔");
            }

            continue;   // 청크 지형은 여기까지
        }

        // ---- 테셀레이션 지형 (스텝 7) ----
        if (TessellatedTerrainRenderer* tess = object->GetComponent<TessellatedTerrainRenderer>())
        {
            if (input.GetKeyDown(VK_TAB))
            {
                tess->ToggleWireframe();
                dxutil::DebugLog(L"[Tess] 와이어프레임 %s", tess->IsWireframe() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown(VK_OEM_PLUS) || input.GetKeyDown(VK_ADD))
                tess->AdjustMaxFactor(4.0f);
            if (input.GetKeyDown(VK_OEM_MINUS) || input.GetKeyDown(VK_SUBTRACT))
                tess->AdjustMaxFactor(-4.0f);
            if (input.GetKeyDown('N'))
                tess->Regenerate(static_cast<unsigned>(TimeManager::Get().GetFrameCount() * 2654435761u + 31u));
            if (input.GetKeyDown('U'))
            {
                tess->ToggleGpuGeneration();
                dxutil::DebugLog(L"[Tess] 높이맵 생성 : %s", tess->IsGpuGeneration() ? L"GPU" : L"CPU");
            }
            if (input.GetKeyDown('R'))
                tess->CycleHeightMapSize();

            continue;
        }

        // ---- 물 (S76~S78) ----
        if (WaterRenderer* water = object->GetComponent<WaterRenderer>())
        {
            if (input.GetKeyDown('X'))
            {
                water->ToggleReflection();
                dxutil::DebugLog(L"[Water] 반사 패스 %s", water->IsReflectionEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('Z'))
            {
                water->ToggleRefraction();
                dxutil::DebugLog(L"[Water] 굴절 · 깊이 %s", water->IsRefractionEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('U'))
            {
                water->ToggleFlow();
                dxutil::DebugLog(L"[Water] 흐름 %s", water->IsFlowEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown('1'))
                water->ToggleRippleDebug();
            if (input.GetKeyDown('R'))
            {
                water->ToggleRain();
                dxutil::DebugLog(L"[Water] 빗방울 %s", water->IsRainEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown(VK_OEM_4)) water->SetWaterLevel(water->GetWaterLevel() - 2.0f);   // [
            if (input.GetKeyDown(VK_OEM_6)) water->SetWaterLevel(water->GetWaterLevel() + 2.0f);   // ]

            continue;
        }

        // ---- 하늘 (스텝 8~9) ----
        if (SkyRenderer* sky = object->GetComponent<SkyRenderer>())
        {
            if (input.GetKeyDown('V'))
            {
                sky->ToggleClouds();
                dxutil::DebugLog(L"[Sky] 구름 %s", sky->AreCloudsEnabled() ? L"켬" : L"끔");
            }
            if (input.GetKeyDown(VK_OEM_PLUS) || input.GetKeyDown(VK_ADD))
                sky->SetCloudCoverage((std::min)(0.95f, sky->GetCloudCoverage() + 0.08f));
            if (input.GetKeyDown(VK_OEM_MINUS) || input.GetKeyDown(VK_SUBTRACT))
                sky->SetCloudCoverage((std::max)(0.05f, sky->GetCloudCoverage() - 0.08f));
            if (input.GetKeyDown('O'))
            {
                sky->ToggleFog();
                dxutil::DebugLog(L"[Sky] 안개 %s", sky->IsFogEnabled() ? L"켬" : L"끔");
            }

            continue;
        }

        // ---- 단일 메시 지형 (스텝 1~4) ----
        TerrainRenderer* terrain = object->GetComponent<TerrainRenderer>();
        if (!terrain) continue;

        // Tab 하나로 표시 모드를 순환한다. (예전의 G / T / B 를 합쳤다)
        if (input.GetKeyDown(VK_TAB))
        {
            terrain->CycleDisplayMode();
            dxutil::DebugLog(L"[Terrain] 표시 모드 : %s", terrain->GetDisplayModeName());
        }

        if (input.GetKeyDown('P'))          // 펄린 <-> 값 노이즈
        {
            terrain->ToggleNoiseType();
            dxutil::DebugLog(L"[Terrain] 노이즈 : %s",
                             terrain->GetNoiseType() == terrain::NoiseType::Perlin ? L"펄린(그래디언트)" : L"값(value)");
        }

        if (input.GetKeyDown('T'))          // 트라이플래너
        {
            terrain->ToggleTriplanar();
            dxutil::DebugLog(L"[Terrain] 트라이플래너 %s", terrain->IsTriplanarEnabled() ? L"켬" : L"끔");
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

    const wchar_t* showcaseName = L"메인 메뉴";
    if (m_currentShowcase >= 0 && m_currentShowcase < static_cast<int>(m_showcases.size()))
        showcaseName = m_showcases[m_currentShowcase].title.c_str();

    wchar_t title[320];
    _snwprintf_s(title, _countof(title), _TRUNCATE,
                 L"DirectXProj — %s | FPS %.0f | dt %.3fms | GameObject %zu | Component %zu | 선택 %S",
                 showcaseName,
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
