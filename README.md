# DirectXProj — 2차 구현

`ChatGTP_DirectXProj_학습_프롬프트_가이드.md` 하나만 보고 처음부터 다시 작성한 DirectX 11 / C++ 2D 게임 프레임워크다.
가이드의 과제 1~6과 확장 과제(`.cso` 캐시 로드)까지 모두 구현되어 있다.

## 빌드 / 실행

```bash
MSBuild.exe DirectXProj/DirectXProj.vcxproj /p:Configuration=Debug /p:Platform=x64
```

또는 `DirectXProj.sln` 을 Visual Studio 로 열고 F5. 툴셋은 설치된 VS 에 맞춰 자동 선택된다(VS2022 = v143, VS2026 = v145).

실행 파일은 `DirectXProj/Build/<Configuration>/DirectXProj.exe` 에 생성되고,
`Shaders/` 와 `Assets/` 는 작업 폴더 → exe 폴더 → 상위 폴더 순으로 탐색되므로 어느 쪽에서 실행해도 동작한다.

## 조작

| 키 | 동작 |
| --- | --- |
| **마우스 좌클릭** | **피킹 — 커서 아래 스프라이트를 선택(노란 외곽선). 빈 곳이면 해제** |
| **마우스 좌버튼 드래그** | **선택한 오브젝트를 커서를 따라 이동. 부모-자식 관계는 그대로 유지된다** |
| **마우스 우클릭** | **선택 해제** |
| **H / I** | **Hierarchy / Inspector 패널 켜기·끄기** |
| **Hierarchy 줄 클릭** | **그 오브젝트 선택. ▶/▼ 로 접고 펴기** |
| **Hierarchy 줄 드래그 → 다른 줄** | **그 오브젝트의 자식으로 이동** |
| **Hierarchy 줄 드래그 → 목록 아래 빈 곳** | **부모에서 떼어 루트로 분리** |
| **Inspector 칸 클릭** | **이름 / 위치 / 회전 / 스케일 편집. Enter 확정, Esc 취소** |
| WASD / 방향키 | 이동 |
| Q, E | Z축 회전 |
| Z, X | 크기 조절 |
| R | 로컬 값 초기화 |
| F1, F2 | 자식 GameObject 추가 / 삭제 |
| F5, F9 | 씬 저장 / 불러오기 (`Saves/scene.json`) |
| ESC | 종료 |

## 폴더 구조

```
DirectXProj/
├─ Assets/     player.png, child.png
├─ Shaders/    SpriteVS.hlsl, SpritePS.hlsl   (런타임 컴파일, .cso 캐시 생성)
├─ Saves/      scene.json                     (F5 저장 결과)
└─ src/
   ├─ Core/      stdafx, Window, Graphics, Game, TimeManager, ObjectRegistry
   ├─ Engine/    Component, GameObject, Transform, SpriteRenderer,
   │             Scene, SceneManager, ComponentFactory, Picker
   ├─ Graphics/  Vertex, Texture, TextureManager, Shader, ShaderManager
   ├─ Input/     InputManager
   ├─ Utils/     Json, StringUtil, IdGenerator, Paths
   ├─ Editor/    EditorStyle, HierarchyPanel, InspectorPanel
   └─ Game/      PlayerController                (데모용 샘플 컴포넌트)
```

## 과제 대응표

| 과제 | 핵심 파일 |
| --- | --- |
| 1. 창 / D3D11 / 루프 / 입력 / 컴포넌트 | `Core/Window`, `Core/Graphics`, `Core/Game`, `Input/InputManager`, `Engine/Component`, `Engine/GameObject`, `Engine/Scene`, `Engine/SceneManager` |
| 2. 시간 / 텍스처 / 스프라이트 | `Core/TimeManager`, `Graphics/Texture`, `Graphics/TextureManager`, `Engine/SpriteRenderer`, `Shaders/Sprite*.hlsl` |
| 3. 계층 Transform / 지연 변경 | `Engine/Transform`, `Engine/GameObject::SetParent`, `ProcessPendingChanges` |
| 4. 타입 조회 / ID / JSON 저장 | `Engine/GameObject.h` 템플릿, `Utils/IdGenerator`, `Core/ObjectRegistry`, `Scene::Save`, `Utils/Json` |
| 5. JSON 로드 / Factory | `Engine/ComponentFactory`, `Scene::Load` (2단계), `Game::RegisterComponentTypes` |
| 6. HLSL 런타임 컴파일 / Hot Reload | `Graphics/Shader`, `Graphics/ShaderManager`, `HOT_RELOAD_ENABLED` |

## 설계 메모

- **입력** — Win32 메시지는 pending 큐에 쌓고 `InputManager::Update()` 에서 한 번에 반영한다. 그래서 "메시지 처리 → 입력 갱신" 순서에서도 Down/Up 이 정확히 한 프레임만 참이다.
- **Transform** — 로컬 값이나 부모 관계가 바뀔 때만 Dirty 를 세우고 자식에게 전파한다. 재부모화는 기존 월드 행렬과 새 부모의 역행렬로 로컬을 구해 `XMMatrixDecompose` 로 분해한다.
- **지연 변경** — Component 추가/삭제, GameObject 생성/삭제 모두 큐를 거쳐 프레임 끝에서만 컨테이너를 수정한다. 씬 전체를 갈아엎는 `Scene::Load` 는 렌더까지 끝난 뒤 `Game::HandleFrameEndCommands()` 에서 실행한다.
- **Shader Hot Reload** — 새 리소스를 임시로 전부 만든 뒤 모두 성공했을 때만 교체한다. 실패하면 마지막 성공 셰이더를 계속 쓰므로 화면이 깨지지 않는다. Release 빌드에서는 `std::filesystem` 감시 코드가 컴파일에서 제외된다.
- **JSON** — 외부 라이브러리 없이 `Utils/Json` 에 파서와 직렬화기를 직접 구현했다. 객체는 삽입 순서를 유지하고, 위치/회전/스케일 같은 짧은 숫자 배열은 한 줄로 출력한다.
- **에셋 폴백** — 이미지 파일을 찾지 못하면 체커보드 텍스처를 만들어 실행이 멈추지 않게 한다.
- **피킹** — `Graphics::ScreenToWorld`가 마우스 클라이언트 좌표를 NDC로 바꾼 뒤 `View * Projection`의 역행렬로 월드 좌표를 구한다. 직교 투영이라 x·y는 깊이와 무관하므로 z=0 평면의 점으로 다룬다. `SpriteRenderer::HitTest`는 그 점을 **월드 행렬의 역행렬**로 스프라이트 로컬 공간에 되돌려 Quad 범위와 비교하므로 회전·스케일·부모 변환이 모두 반영된다. 겹치면 나중에 그려진 쪽이 선택된다.
- **선택 하이라이트** — 같은 Quad를 외곽선 두께만큼 크게, 텍스처의 알파만 쓰는 단색 실루엣으로 먼저 그리고 그 위에 원본을 겹친다(`gParams.x`로 픽셀 셰이더 분기). 깊이 버퍼 없이 화가 알고리즘만으로 외곽선이 된다.
- **선택 상태는 ID로 보관** — `Game`은 선택된 오브젝트를 포인터가 아니라 `uint64_t` ID로 들고 있다. 선택한 오브젝트를 F2로 삭제하거나 F9로 씬을 다시 로드해도 잘못된 포인터를 쓰지 않는다.
- **드래그 이동** — 버튼을 누른 순간 `오브젝트 월드 위치 - 커서 월드 위치`를 저장해 두고, 매 프레임 커서 위치에 그 차이를 더한다. 잡은 지점이 튀지 않는다. 목표 월드 위치는 `Transform::SetWorldPosition`이 **부모 월드의 역행렬**로 되돌려 로컬 위치로 바꾸므로, 부모가 회전·확대되어 있어도 자식이 커서를 정확히 따라간다. `SetWorldMatrix`와 달리 분해(decompose)를 하지 않아 회전·스케일에 오차가 쌓이지 않는다.
- **드래그는 계층을 건드리지 않는다** — 그냥 드래그하면 부모-자식 관계가 그대로 유지된다(에디터의 일반적인 동작). 자식으로 남아 있으므로 이후 부모가 회전·이동하면 함께 끌려가는데, 이는 계층 구조상 맞는 동작이다.
- **계층 변경은 Hierarchy 패널에서만** — 씬 드래그는 이동 전용이고, 부모를 바꾸는 것은 Hierarchy에서 줄을 끌어다 놓을 때뿐이다. 되돌릴 수 없는 조작을 우연히 일으키지 않게 분리했다. 재부모화는 `SetParent(newParent, worldPositionStays: true)`라 화면에 보이는 위치·회전·크기가 그대로 유지된다.
- **에디터 UI는 GDI 오버레이** — 백버퍼를 `DXGI_FORMAT_B8G8R8A8_UNORM` + `DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE`로 만들고 `IDXGISurface1::GetDC`로 DC를 빌려 씬 위에 직접 그린다. 폰트 텍스처나 텍스트 렌더링 파이프라인이 따로 필요 없고 한글 이름도 그대로 나온다. DC는 프레임당 한 번만 잡아 두 패널이 나눠 쓴다.
- **텍스트 입력 캡처** — 인스펙터가 글자를 받는 동안 `InputManager::SetTextCaptureActive(true)`로 게임 쪽 키 조회를 막는다. 이름에 'D'를 쳐도 캐릭터가 움직이지 않는다. Enter·Backspace·Esc는 `WM_CHAR`로 함께 들어오므로 편집 중 Esc는 취소, 편집이 아닐 때 Esc는 종료로 갈린다.
