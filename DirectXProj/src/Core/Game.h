#pragma once
#include "Core/stdafx.h"
#include "Editor/HierarchyPanel.h"
#include "Editor/InspectorPanel.h"

class Window;
class Graphics;
class GameObject;

// =============================================================
// Game (과제 1, 5, 6)
//  시스템 초기화 순서와 게임 루프를 담당한다.
//
//  게임 루프
//      메시지 처리 → 입력 갱신 → (Hot Reload 검사) → 씬 Update
//      → BeginFrame → 씬 Render → EndFrame → 프레임 끝 정리
// =============================================================
class Game
{
public:
    Game();
    ~Game();

    bool Initialize(HINSTANCE hInstance, int width = 1280, int height = 720);
    int  Run();
    void Shutdown();

private:
    void RegisterComponentTypes();     // ComponentFactory 등록 (과제 5)
    // 터레인 씬의 높이 공급원. 스텝 1/2/3 이 이 값만 다르다.
    enum class TerrainMode { Flat, Noise, Image };

    // 씬 구성 함수들. 메뉴 항목 하나가 이 중 하나를 부른다.
    void BuildTerrainScene(TerrainMode mode, const std::string& sceneName);
    void BuildSpriteDemoScene();

    // 메뉴에 올라가는 항목 하나.
    //  스텝이 늘어나면 SetupMenu 에 한 줄만 더하면 된다.
    struct Showcase
    {
        std::wstring          title;
        std::wstring          description;
        std::function<void()> build;
    };

    void SetupShowcaseList();          // 메뉴에 올릴 목록을 만든다
    void BuildMenuScene();             // 메인 메뉴도 그냥 하나의 씬이다

    // 씬 전환 요청. 순회 중에 씬을 갈아엎으면 안 되므로 프레임 끝에 처리한다.
    void RequestScene(int index);      // kMenuScene = 메뉴, 0 이상 = 쇼케이스
    void ProcessPendingSceneChange();

    static constexpr int kNoRequest = -2;
    static constexpr int kMenuScene = -1;
    void UpdatePickingAndDrag();       // 마우스 피킹 / 선택 하이라이트 / 드래그 이동
    void UpdateEditorUI();             // Hierarchy / Inspector 입력 처리
    void DrawOverlayUI();              // 메뉴 + 에디터 패널을 한 번의 GDI 오버레이로 그린다
    void ApplyReparent(uint64_t dragId, uint64_t newParentId);
    GameObject* GetSelectedObject() const;
    void HandleFrameEndCommands();     // 저장/로드처럼 프레임 경계에서만 안전한 작업
    void UpdateWindowTitle();

    std::unique_ptr<Window>   m_window;
    std::unique_ptr<Graphics> m_graphics;

    bool m_comInitialized = false;
    bool m_running = false;

    std::vector<Showcase> m_showcases;

    // 메뉴도 씬이라 별도 상태 enum 이 없다.
    //  m_currentShowcase == kMenuScene 이면 지금 보고 있는 씬이 메뉴다.
    int  m_currentShowcase = kMenuScene;
    int  m_pendingScene = kNoRequest;
    bool m_showEditorPanels = false;   // 메뉴 씬에서는 Hierarchy / Inspector 를 숨긴다

    std::wstring m_savePath;
    float m_titleTimer = 0.0f;

    // 선택된 오브젝트는 포인터가 아니라 ID 로 들고 있는다. (과제 4 의 ObjectRegistry 규칙)
    // 그래야 선택한 오브젝트가 삭제되거나 씬을 다시 로드해도 잘못된 포인터를 쓰지 않는다.
    uint64_t m_selectedId = 0;
    bool     m_terrainSceneActive = true;

    // 드래그 상태. 누른 순간의 "오브젝트 위치 - 커서 위치" 차이를 기억해 두었다가
    // 매 프레임 커서에 그 차이를 더한다. 그래야 잡은 지점이 튀지 않는다.
    bool              m_dragging = false;
    DirectX::XMFLOAT3 m_dragOffset{ 0.0f, 0.0f, 0.0f };

    // 에디터 UI
    HierarchyPanel m_hierarchy;
    InspectorPanel m_inspector;
};
