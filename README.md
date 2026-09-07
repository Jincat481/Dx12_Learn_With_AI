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

프로그램을 켜면 **메뉴**가 먼저 뜬다. 스텝 하나가 항목 하나다. 고르면 그 씬으로 들어가고 `ESC` 로 돌아온다.

```
터레인 · 스텝 1  평면 그리드     격자 메시 생성 · 원근 카메라 · 깊이 버퍼 (S27~S35)
터레인 · 스텝 2  하이트맵 지형   fBm 노이즈 높이 · 중앙 차분 법선 · 램버트 조명 (S36~S40)
2D 스프라이트 데모               계층 Transform · 피킹 · 드래그 · Hierarchy / Inspector
종료
```

현재 들어가 있는 스텝은 창 제목에 표시된다.

| 입력 | 동작 |
| --- | --- |
| ↑ ↓ / 마우스 | 항목 이동 |
| Enter / 클릭 | 선택 |
| ESC (메뉴에서) | 종료 |

### 터레인 쇼케이스

| 입력 | 동작 |
| --- | --- |
| 마우스 가운데 버튼 드래그 | 궤도 회전 (yaw / pitch) |
| 마우스 휠 | 줌 |
| WASD | 카메라 타깃 수평 이동 |
| Q / E | 타깃 높이 |
| F | 카메라 원점으로 리셋 |
| G | 와이어프레임 토글 |
| T | 평면 ↔ 하이트맵 토글 |
| N | 새 seed 로 지형 재생성 |
| ESC | 메뉴로 복귀 |

### 스프라이트 데모 씬

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
├─ Shaders/    SpriteVS/PS.hlsl, TerrainVS/PS.hlsl   (런타임 컴파일, .cso 캐시 생성)
├─ Saves/      scene.json                     (F5 저장 결과)
└─ src/
   ├─ Core/      stdafx, Window, Graphics, Game, TimeManager, ObjectRegistry
   ├─ Engine/    Component, GameObject, Transform, SpriteRenderer, Camera,
   │             Scene, SceneManager, ComponentFactory, Picker
   ├─ Graphics/  Vertex, Mesh, Texture, TextureManager, Shader, ShaderManager
   ├─ Terrain/   HeightField, TerrainMeshBuilder, TerrainRenderer
   ├─ Input/     InputManager
   ├─ Utils/     Json, StringUtil, IdGenerator, Paths
   ├─ Editor/    EditorStyle, MenuScreen, HierarchyPanel, InspectorPanel
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

## 터레인 쇼케이스

단계별로 쌓아 올린다. 각 스텝은 앞 스텝의 구조를 그대로 쓴다.

| 스텝 | 내용 | 상태 |
| --- | --- | --- |
| 1 | 평면 그리드 메시 + 원근 카메라 + 깊이 버퍼 | **완료** |
| 2 | 하이트맵으로 높이 주기, 법선 계산, 높이 색상 램프 | **완료** |
| 3 | 텍스처 스플래팅(레이어 블렌딩) | 예정 |
| 4 | 조명 (Directional + 그림자) | 예정 |
| 5 | 청크 분할과 LOD | 예정 |
| 6 | 지형 편집 브러시 | 예정 |

### 스텝 1 에서 알아야 할 키워드

`ChatGTP_DirectXProj_학습_프롬프트_가이드.md` 의 S01~S26 에 이어서 번호를 붙인다.

| 번호 | 키워드 | 왜 필요한가 | 코드 위치 |
| --- | --- | --- | --- |
| **S27** | 3D 좌표계와 뷰 행렬 — 왼손 좌표계, `XMMatrixLookAtLH`, eye/target/up, 구면 좌표(yaw·pitch·distance) → 눈 위치 | 2D 는 카메라가 사실상 고정이었다. 3D 는 "어디서 어느 쪽을 보는가" 를 행렬로 만들어야 한다 | `Engine/Camera.cpp` |
| **S28** | 원근 투영 — `XMMatrixPerspectiveFovLH`, FOV, **aspect ratio**, near/far, 원근 나눗셈(w) | 직교 투영은 멀어져도 크기가 같다. 지형의 거리감은 원근 투영에서만 나온다. near 를 너무 작게 잡으면 깊이 정밀도가 무너진다 | `Engine/Camera.cpp` |
| **S29** | 깊이 버퍼 — `D24_UNORM_S8_UINT`, DSV, `ClearDepthStencilView`, DepthFunc(LESS), depth write, Z-fighting | 3D 는 그린 순서가 아니라 거리로 앞뒤가 정해진다. 매 프레임 1.0 으로 지우지 않으면 이전 프레임 깊이가 남는다 | `Core/Graphics::CreateDepthBuffer` |
| **S30** | 그리드 메시 생성 — 칸 수 vs 정점 수(`cells+1`), 2차원 격자를 1차원 배열에 담는 인덱싱 `r*(cellsX+1)+c`, 칸 하나 = 삼각형 2개 = 인덱스 6개 | 터레인의 뼈대다. 이 공식을 손으로 못 쓰면 하이트맵도 LOD 도 못 만든다 | `Terrain/TerrainMeshBuilder.cpp` |
| **S31** | 정점 레이아웃 확장 — POSITION/NORMAL/TEXCOORD, offset 과 stride(32바이트), Input Layout 과 HLSL semantic 의 계약 | 스프라이트의 정점(20바이트)과 다른 구조체를 쓰므로 셰이더도 레이아웃도 따로 만들어야 한다 | `Graphics/Vertex.h`, `Shaders/TerrainVS.hlsl` |
| **S32** | 래스터라이저 상태와 winding — FillMode(Solid/Wireframe), CullMode, `FrontCounterClockwise`, 시계 방향이 앞면 | 인덱스 순서를 반대로 넣으면 지형이 통째로 사라진다. 원인을 못 찾으면 몇 시간을 날린다 | `Core/Graphics::CreateMeshPipeline` |
| **S33** | 인덱스 버퍼 폭 — 16비트(`R16_UINT`)는 정점 65,536개가 한계, 32비트(`R32_UINT`)는 메모리 2배 | 64×64 격자만 해도 정점이 4,225개다. 256×256 이면 16비트로는 못 담는다 | `Graphics/Mesh.cpp` |
| **S34** | UV 매핑과 타일링 — 격자 전체를 0~1 로 정규화, `uvTiling` 으로 반복 | 다음 스텝의 텍스처 스플래팅이 이 UV 위에 올라간다 | `Terrain/TerrainMeshBuilder.cpp` |
| **S35** | 화면 공간 미분 — `frac`, `fwidth`, `saturate` 로 굵기가 일정한 격자선 그리기 | 텍스처 없이 픽셀 셰이더만으로 격자를 그린다. 거리와 무관하게 선 굵기가 유지되고 계단 현상도 줄어든다 | `Shaders/TerrainPS.hlsl` |

### 스텝 2 에서 알아야 할 키워드

| 번호 | 키워드 | 왜 필요한가 | 코드 위치 |
| --- | --- | --- | --- |
| **S36** | 하이트맵 — 높이 함수 `h(x, z)`, 이미지 하이트맵 vs 절차적 생성, 해상도와 계단 현상 | 격자의 y 만 바꾸면 지형이 된다. 스텝 1 의 구조를 그대로 두고 값만 채운다 | `Terrain/HeightField.h` |
| **S37** | 값 노이즈와 fBm — 격자점 해시, `smoothstep` 보간, octaves / frequency / **persistence**(진폭 감소) / **lacunarity**(주파수 증가) | 옥타브를 겹쳐야 큰 산맥 위에 작은 굴곡이 얹힌 자연스러운 모양이 나온다. 한 층만 쓰면 밋밋하다 | `Terrain/HeightField.cpp` |
| **S38** | 법선 계산 — 중앙 차분으로 기울기 구하기, `normal = normalize(-dh/dx, 1, -dh/dz)`, 정점 법선 보간 | **법선이 없으면 조명이 안 된다.** 높이를 줘도 전부 같은 밝기라 지형이 평면처럼 보인다 | `HeightField::SampleNormal` |
| **S39** | 램버트 확산 조명 — `N·L`, 방향광, 환경광(ambient), `saturate` | 지형의 굴곡은 밝기 차이로 보인다. 가장 단순하면서 효과가 큰 조명 모델 | `Shaders/TerrainPS.hlsl` |
| **S40** | 높이 기반 색상 램프 — 높이 정규화, `smoothstep` 으로 구간 blend | 텍스처 없이 풀 → 바위 → 눈 으로 고도감을 준다. 스텝 3 의 스플래팅으로 가는 징검다리 | `HeightColor()` |

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
- **메뉴와 쇼케이스는 별개 상태** — `Game` 이 `AppState::Menu` / `Showcase` 두 상태를 오간다. 메뉴 상태에서는 씬을 아예 돌리지 않고 오버레이만 그린다. 항목을 고를 때 씬을 만들고, ESC 로 나올 때 `SceneManager::Shutdown` 으로 정리한다.
- **메뉴는 데이터 기반** — 항목을 `{ 제목, 설명, 씬 구성 람다 }` 목록(`m_showcases`)으로 들고 있고 메뉴 화면은 그 목록을 그릴 뿐이다. 스텝이 늘어나면 `Game::SetupMenu()` 에 `push_back` 한 줄만 더하면 된다. 분기문(`if index == 0`)이 없으므로 항목이 늘어도 진입 코드는 그대로다.
- **카메라는 Transform 을 소유한다** — 궤도 파라미터(타깃·거리·yaw·pitch)로 눈 위치를 계산한 뒤 **매 프레임 자기 GameObject 의 Transform 에 써 넣는다.** 그래서 Inspector 에 실제 카메라 위치가 보인다. 반대로 Inspector 에서 위치를 고치면, 마지막으로 써 넣은 값과 달라진 것을 감지해 거리·각도를 거꾸로 계산한다(양방향 동기화).
- **높이와 법선은 같은 함수에서** — 정점의 y 는 `HeightField::Sample`, 법선은 같은 함수를 좌우/앞뒤로 한 칸씩 샘플링한 중앙 차분으로 구한다. 삼각형 면법선을 모아 평균 내는 방식보다 코드가 짧고 이음매가 매끄럽다.
- **2D 와 3D 를 한 프레임에** — `Graphics` 가 직교(스프라이트)와 원근(터레인) 두 벌의 View/Projection 을 따로 들고 있다. 스프라이트는 깊이 테스트를 끄고 그린 순서대로, 메시는 깊이 테스트를 켜고 그린다. 덕분에 기존 2D 경로를 건드리지 않고 3D 를 얹었다.
- **카메라는 컴포넌트** — `Camera` 가 Update 에서 View/Projection 을 계산해 `Graphics::SetCamera3D` 로 넘긴다. 모든 Update 가 끝난 뒤 Render 가 돌기 때문에 같은 프레임 안에서 순서가 보장된다.
- **격자선은 텍스처가 아니라 픽셀 셰이더로** — UV 에 칸 수를 곱하고 `frac`/`fwidth` 로 셀 경계를 찾는다. 텍스처 로딩 없이 10칸마다 굵은 선까지 그릴 수 있고, 카메라가 멀어져도 선 굵기가 일정하다.
- **메시 재생성은 Dirty 일 때만** — `TerrainRenderer` 는 격자 설정이 바뀐 프레임에만 정점 버퍼를 다시 만든다. Transform 의 Dirty Flag 와 같은 규칙이다.
- **텍스트 입력 캡처** — 인스펙터가 글자를 받는 동안 `InputManager::SetTextCaptureActive(true)`로 게임 쪽 키 조회를 막는다. 이름에 'D'를 쳐도 캐릭터가 움직이지 않는다. Enter·Backspace·Esc는 `WM_CHAR`로 함께 들어오므로 편집 중 Esc는 취소, 편집이 아닐 때 Esc는 종료로 갈린다.
