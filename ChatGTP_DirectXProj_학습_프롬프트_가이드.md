# DirectXProj 기능 구현 학습·프롬프트 가이드

## 문서 목적

이 문서는 `기본프레임워크 만들기.md`에 기록된 작업 요청을 학생용 학습 과제로 재구성한 것이다. 각 과제는 **무엇을 완성해야 하는지**를 분명히 하는 프롬프트와, 그 프롬프트를 수행하기 전에 알아야 할 **선행학습(스터디 번호)**을 연결한다.

- 대상: DirectX 11 / C++ 기반 2D 게임 프레임워크
- 구현 기준: `DirectXProj/src` 및 `DirectXProj/Shaders`
- 권장 순서: 과제 1 → 6. 뒤 과제는 앞 과제의 구조와 자원을 전제한다.

> 주의: 이 문서는 설계 문서의 요청을 학습용으로 정리한 것이다. 실제 소스에는 실험적·간략화된 구현도 있다. 특히 과제 6의 `.cso` **저장** 옵션은 있으나, `.cso`를 다시 읽어 `CreateVertexShader`/`CreatePixelShader`로 생성하는 전용 로드 경로는 현재 소스에 구현되어 있지 않다. 이 항목은 학생의 확장 과제로 다룬다.

## 전체 학습 지도

| 과제 | 완성 기능 | 먼저 끝낼 스터디 |
| --- | --- | --- |
| 1 | 창, D3D11 초기화, 게임 루프, 입력, GameObject/Component/Scene | S01~S06 |
| 2 | 시간 측정, 텍스처, Quad 스프라이트 렌더링 | S07~S12 |
| 3 | 부모-자식 Transform, 로컬/월드 좌표, Dirty Flag, 지연 변경 | S13~S16 |
| 4 | 타입 기반 컴포넌트 조회, 다중 컴포넌트, ID 레지스트리, JSON 저장 | S17~S20 |
| 5 | ComponentFactory와 JSON 기반 씬 로드 | S21~S22 |
| 6 | HLSL 런타임 컴파일, 캐시, Hot Reload, 빌드 옵션 | S23~S26 |

---

## 과제 1. 최소 DirectX 게임 프레임워크

### 목표

Win32 창에서 DirectX 11의 파란색 화면을 표시하고, 프레임 단위 게임 루프 안에서 입력·씬·컴포넌트 생명주기가 동작하는 최소 게임 프레임워크를 만든다.

### 작업용 프롬프트

```text
DirectX 11과 C++로 Unity와 유사한 기본 게임 프레임워크를 만들어줘.

- Win32 Window 클래스로 창 생성과 메시지 처리를 구현한다.
- Graphics 클래스에서 D3D11 디바이스, 스왑 체인, 렌더 타깃 뷰, 뷰포트를 초기화하고,
  매 프레임 파란색으로 화면을 Clear한 뒤 Present한다.
- Game 클래스가 Window, Graphics, InputManager, SceneManager를 초기화하고
  메시지 처리 → 입력 갱신 → 씬 Update → BeginFrame → 씬 Render → EndFrame 순서의 게임 루프를 실행한다.
- Component 추상 클래스에 Start, Update, Render, OnDestroy 가상 함수를 둔다.
- GameObject는 생성 시 Transform을 반드시 하나 보유하고, 자신이 가진 Component의 생명주기를 호출한다.
- Scene은 GameObject를 소유하고, SceneManager는 현재 활성 Scene을 관리한다.
- Win32 메시지로 키보드와 마우스 입력을 받아 InputManager에서 Down, Pressed, Up 상태를 조회할 수 있게 한다.
- 소스는 Core, Engine, Graphics, Input, Utils 폴더로 분리한다.
```

### 선행학습

- **S01. C++ 객체 수명과 소유권** — 클래스 분리, 헤더/CPP, 전방 선언, `std::unique_ptr`, RAII를 학습한다. `Scene`이 GameObject를, GameObject가 Component를 소유하는 이유를 설명할 수 있어야 한다.
- **S02. Win32 창과 메시지 루프** — `WNDCLASSEXW`, `CreateWindowExW`, `WndProc`, `PeekMessage`, `DispatchMessage`, `WM_QUIT`의 역할을 실습한다.
- **S03. Direct3D 11 초기화** — `D3D11CreateDeviceAndSwapChain`, back buffer, `ID3D11RenderTargetView`, `OMSetRenderTargets`, `D3D11_VIEWPORT`, `Present`의 연결을 이해한다.
- **S04. 렌더 루프와 프레임 순서** — 입력, 게임 로직, 렌더, 프레임 마감의 책임을 분리한다. Update 중의 변경이 Render에 미치는 시점을 말할 수 있어야 한다.
- **S05. 컴포넌트 아키텍처** — 상속과 가상 함수, GameObject-Component 합성(composition), `Start/Update/Render/OnDestroy` 생명주기 차이를 학습한다.
- **S06. 입력 상태 머신** — `None → Down → Pressed`, `Pressed → Up → None`의 전이를 표로 설계하고, `WM_KEYDOWN`, `WM_KEYUP`, 마우스 메시지를 상태에 반영하는 연습을 한다.

### 구현 확인 지점

- `Core/Window.cpp`: Win32 메시지를 `InputManager`로 전달한다.
- `Core/Graphics.cpp`: D3D11 초기화, Clear, Present를 담당한다.
- `Core/Game.cpp`: 프레임 루프와 시스템 초기화 순서를 조정한다.
- `Engine/Component.*`, `Engine/GameObject.*`, `Engine/Scene.*`, `Engine/SceneManager.*`: 엔진 객체와 생명주기 구조를 제공한다.

---

## 과제 2. 시간·텍스처·2D 스프라이트 렌더링

### 목표

프레임 간 시간과 누적 시간을 제공하고, 이미지 파일을 GPU 텍스처로 올려 Quad에 매핑하여 화면에 스프라이트를 렌더링한다.

### 작업용 프롬프트

```text
기본 프레임워크에 TimeManager와 2D SpriteRenderer를 추가해줘.

- TimeManager는 싱글톤으로 만들고, 고해상도 타이머를 사용해 Delta Time과 Total Time을 매 프레임 갱신한다.
- Texture와 TextureManager를 구현한다. TextureManager는 경로별 캐시를 사용해 같은 텍스처를 중복 로드하지 않는다.
- WIC로 PNG 등의 이미지를 읽고, D3D11 Texture2D와 Shader Resource View(SRV)를 생성한다.
- SpriteRenderer는 Component를 상속하고, 텍스처 크기를 기준으로 한 4개 정점과 6개 인덱스의 Quad를 만든다.
- 각 정점에는 POSITION과 TEXCOORD(UV)를 넣는다.
- HLSL Vertex Shader와 Pixel Shader를 작성한다. WVP 행렬은 상수 버퍼(b0)로 전달하고,
  텍스처(t0)와 샘플러(s0)를 Pixel Shader에 바인딩한다.
- Graphics는 정점/인덱스 버퍼, 셰이더, 상수 버퍼, 샘플러를 설정한 뒤 DrawIndexed(6)를 호출한다.
```

### 선행학습

- **S07. 시간 측정과 Delta Time** — `QueryPerformanceFrequency`, `QueryPerformanceCounter`, 초 단위 변환, 중단점 후 큰 delta time 제한(clamp)을 학습한다.
- **S08. 싱글톤의 책임과 한계** — 전역 접근의 장단점, 초기화 순서, 종료 처리, 테스트 어려움을 이해한다. 이 프로젝트에서는 Manager 계열 접근 방식으로 사용한다.
- **S09. GPU 리소스와 COM 포인터** — D3D11 device/context, `ComPtr`, 버퍼와 텍스처의 수명, CPU 메모리와 GPU 메모리의 차이를 학습한다.
- **S10. 정점·인덱스·UV 좌표** — Triangle List에서 Quad를 두 삼각형으로 만드는 방법, 정점 순서(winding), UV 원점과 이미지 상하 반전 문제를 실습한다.
- **S11. WIC와 텍스처 업로드** — WIC 디코더, 포맷 변환(PBGRA), stride, `D3D11_TEXTURE2D_DESC`, SRV 생성 과정을 학습한다.
- **S12. HLSL 기초와 상수 버퍼** — Vertex/Pixel Shader 역할, semantic, register, 행렬 곱 순서, CPU/HLSL 행렬 전치, 상수 버퍼의 16바이트 정렬 규칙을 학습한다.

### 구현 확인 지점

- `Core/TimeManager.*`: 고해상도 카운터로 시간을 계산한다.
- `Graphics/Texture.*`, `Graphics/TextureManager.*`: WIC 로드와 경로 기반 캐시를 담당한다.
- `Engine/SpriteRenderer.*`: 텍스처 크기 기반 Quad와 버퍼를 생성한다.
- `Core/Graphics.cpp`, `Shaders/SpriteVS.hlsl`, `Shaders/SpritePS.hlsl`: WVP, 텍스처, 샘플러 바인딩과 Draw 호출을 담당한다.

---

## 과제 3. 계층 Transform과 안전한 런타임 변경

### 목표

GameObject를 부모-자식 트리로 구성하고, Transform의 로컬 값과 월드 행렬을 정확히 계산한다. 또한 반복 중 컨테이너를 훼손하지 않도록 Component 추가·삭제를 프레임 끝에 처리한다.

### 작업용 프롬프트

```text
GameObject에 계층 구조와 최적화된 Transform 시스템을 추가해줘.

- GameObject는 부모 하나와 자식 목록을 가진다. SetParent로 재부모화할 때 기존 부모에서 분리하고 새 부모에 연결한다.
- Transform은 local position, quaternion rotation, local scale을 보관하고, 월드 행렬을 캐시한다.
- 월드 행렬은 local S*R*T와 부모 월드 행렬을 결합해 계산한다.
- 로컬 값이나 부모 관계가 바뀌었을 때만 Dirty Flag를 설정하고, GetWorldMatrix 요청 시 필요한 경우에만 재계산한다.
- 부모 Transform이 Dirty가 되면 모든 자식 Transform도 Dirty가 되게 한다.
- 재부모화 시에는 기존 월드 모양을 유지하도록 기존 월드 행렬과 새 부모의 역행렬로 새 로컬 행렬을 구하고 분해한다.
- AddComponent와 RemoveComponent는 Update/Render 순회 중 컨테이너를 직접 수정하지 않고 큐에 기록한다.
- 삭제 예약된 Component는 즉시 Update/Render에서 제외하고, 모든 렌더가 끝난 후 ProcessPendingChanges에서 일괄 반영한다.
- GameObject Destroy도 즉시 메모리를 해제하지 말고 비활성 표시 후 안전한 시점에 Scene에서 제거한다.
```

### 선행학습

- **S13. 3D 변환 수학** — 벡터, 행렬, SRT 합성, 좌표계, 행렬 곱 순서, 역행렬과 `XMMatrixDecompose`를 학습한다.
- **S14. 쿼터니언** — Euler angle의 한계와 쿼터니언 회전, identity quaternion, 행렬과 쿼터니언 변환을 이해한다.
- **S15. 트리 자료구조와 전파** — 부모 포인터·자식 배열, 순회, 순환 참조 방지, 부모 변경 시 자식에게 상태를 전파하는 방법을 학습한다.
- **S16. Dirty Flag·지연 변경(Deferred Mutation)** — 캐시 무효화와 lazy evaluation, 컨테이너 순회 중 `erase`가 위험한 이유, 명령 큐/보류 목록을 이용한 안전한 변경 시점을 학습한다.

### 구현 확인 지점

- `Engine/Transform.*`: local S/R/T, 월드 행렬 캐시, Dirty 전파를 구현한다.
- `Engine/GameObject.cpp`: `SetParent`에서 월드 변환 보존을 위한 역행렬·분해를 수행한다.
- `Engine/GameObject.*`, `Engine/Scene.cpp`: pending add/remove과 비활성 GameObject 제거를 프레임 끝에 처리한다.

---

## 과제 4. 빠른 컴포넌트 조회, 고유 ID, JSON 저장

### 목표

Component를 타입별로 빠르게 찾아 여러 개를 관리하고, 객체·컴포넌트에 고유 ID를 부여하며, 현재 Scene의 구조와 데이터를 JSON으로 저장한다.

### 작업용 프롬프트

```text
Component 조회와 Scene 저장 기능을 확장해줘.

- GameObject의 Component 저장 구조를 단순 vector 순회 대신
  unordered_map<type_index, vector<unique_ptr<Component>>> 형태로 바꾼다.
- GetComponent<T>()는 해당 타입의 첫 Component를 빠르게 반환하고,
  GetComponents<T>()는 같은 타입으로 등록된 모든 Component를 반환한다.
- GameObject와 Component 생성 시 uint64_t 고유 ID를 발급한다.
- ObjectRegistry를 싱글톤으로 만들고 ID → GameObject, ID → Component 조회를 제공한다.
- Scene::Save(path)는 scene name, GameObject ID/name/parent_id, Transform의 로컬 위치·회전·스케일,
  그리고 각 Component의 타입과 속성을 JSON으로 저장한다.
- Component에는 ToJson/FromJson 가상 함수를 두고, Transform과 SpriteRenderer가 자신의 추가 데이터를 직렬화하도록 한다.
- JSON은 사람이 읽을 수 있게 들여쓰기를 적용해 저장하고, 텍스처 경로의 UTF-8 변환을 처리한다.
```

### 선행학습

- **S17. C++ 템플릿과 RTTI** — `template`, `typeid`, `std::type_index`, `dynamic_cast`의 쓰임과 비용을 학습한다. 이 구조가 왜 `unordered_map` 키에 타입 정보를 쓰는지 설명할 수 있어야 한다.
- **S18. 해시 테이블과 복잡도** — `vector` 선형 탐색과 `unordered_map` 평균 O(1) 조회의 차이, 충돌·버킷·키 설계를 이해한다.
- **S19. 고유 ID와 레지스트리 패턴** — 단조 증가 ID, 포인터 수명, 등록/해제 타이밍, 삭제된 객체를 조회하지 않기 위한 규칙을 학습한다.
- **S20. JSON 직렬화** — 객체 그래프를 트리가 아닌 ID 참조로 저장하는 이유, 다형성 타입 이름, UTF-8, 저장 실패 처리, 저장 포맷 버전 관리의 필요성을 학습한다.

### 구현 확인 지점

- `Engine/GameObject.h`: 타입 인덱스에서 Component 배열로 연결되는 자료구조와 `GetComponent(s)` 템플릿을 제공한다.
- `Utils/IdGenerator.*`, `Core/ObjectRegistry.*`: ID 발급 및 조회 등록소를 담당한다.
- `Engine/Component.*`, `Engine/Transform.cpp`, `Engine/SpriteRenderer.cpp`: 공통/개별 JSON 데이터를 직렬화한다.
- `Engine/Scene.cpp`: `Scene::Save`가 Scene 전체를 JSON 파일로 기록한다.

---

## 과제 5. JSON 씬 로드와 ComponentFactory

### 목표

저장한 JSON으로 GameObject, Component, Transform, 부모-자식 관계를 다시 만들고, ID의 연속성과 초기화 순서를 보장한다.

### 작업용 프롬프트

```text
저장된 JSON Scene을 완전하게 복원하는 Load 기능을 추가해줘.

- ComponentFactory에 문자열 타입 이름 → Component 생성 함수 등록 기능을 구현한다.
- 게임 시작 시 Transform, SpriteRenderer 등 로드 가능한 Component 타입을 Factory에 등록한다.
- Scene::Load(path)는 기존 Scene 객체를 정리한 후 JSON을 읽는다.
- 1차 순회에서 JSON의 ID와 이름으로 모든 GameObject를 만들고, Transform과 Component 데이터를 복원한다.
- 2차 순회에서 parent_id를 이용해 부모-자식 관계를 연결한다. 참조 대상이 아직 없어서 실패하지 않도록 반드시 두 단계로 구현한다.
- 로드된 GameObject 중 최대 ID를 이용해 ID 생성기의 다음 값을 갱신한다.
- 그래픽 리소스가 필요한 Component는 Graphics 초기화 후 Initialize를 호출하고, 그 다음 Start를 호출한다.
- 파일 없음, JSON 파싱 실패, 등록되지 않은 Component 타입, 잘못된 parent_id를 안전하게 처리하고 오류를 보고한다.
```

### 선행학습

- **S21. Factory·등록(Registration) 패턴** — 생성자를 직접 알 수 없는 다형 객체를 문자열/함수 객체로 만드는 방법, `std::function`, lambda, `unique_ptr<Component>` 반환을 학습한다.
- **S22. 객체 그래프 역직렬화** — 두 단계 로드(create → link), 전방 참조, ID 재사용 여부, 초기화 순서(Deserialize → GPU Initialize → Start)를 학습한다.

### 구현 확인 지점

- `Engine/ComponentFactory.*`: 타입 이름별 Component 생성 함수를 관리한다.
- `Engine/Scene.cpp`: 1차 생성·역직렬화 후 2차 부모 연결이라는 두 단계 Load를 수행한다.
- `Core/Game.cpp`: Factory 등록과 Scene/Graphics 초기화 흐름을 제공한다.

---

## 과제 6. 런타임 HLSL 컴파일과 Shader Hot Reload

### 목표

HLSL 소스를 실행 중 컴파일해 D3D11 Shader 객체를 만들고, 개발 중 파일 변경을 감지해 안전하게 교체한다. 배포용으로는 캐시와 Hot Reload를 설정으로 분리한다.

### 작업용 프롬프트

```text
셰이더 로딩 파이프라인을 개발/배포에 맞게 확장해줘.

- Shader::Load는 HLSL 파일을 D3DCompileFromFile로 컴파일하고, 생성된 Blob으로
  CreateVertexShader, CreatePixelShader, CreateInputLayout을 수행한다.
- 컴파일 실패 시 error blob의 메시지를 디버그 출력과 사용자 오류 메시지로 제공하고,
  기존에 정상 동작하던 셰이더는 유지한다.
- cacheCompiled 옵션이 켜졌을 때 HLSL 컴파일 Blob을 .cso 파일로 저장한다.
- 추가 과제로, .cso가 최신이고 캐시 사용 옵션이 켜져 있으면 D3DReadFileToBlob으로 .cso를 읽어
  CreateVertexShader/CreatePixelShader/CreateInputLayout을 수행하는 캐시 로드 경로를 구현한다.
- ShaderManager는 등록된 Shader의 vertex/pixel HLSL 최종 수정 시간을 저장한다.
- 매 프레임 또는 일정 주기마다 수정 시간을 비교해 변경된 Shader만 Reload한다.
- Hot Reload 기능은 HOT_RELOAD_ENABLED 같은 컴파일 매크로로 켜고 끌 수 있게 하며,
  Release 빌드에서는 감시·파일 시스템 검사 코드가 제외되게 한다.
- Reload 실패 시에는 마지막 성공 셰이더를 계속 사용하고, 성공했을 때만 새 셰이더와 수정 시간을 확정한다.
```

### 선행학습

- **S23. HLSL 컴파일과 Blob** — entry point, shader profile(`vs_5_0`, `ps_5_0`), include 처리, `ID3DBlob`, 컴파일 플래그(Debug/최적화)을 학습한다.
- **S24. Input Layout 계약** — C++ `Vertex` 구조체의 메모리 레이아웃과 HLSL semantic(`POSITION`, `TEXCOORD`)이 일치해야 하는 이유, offset과 format을 학습한다.
- **S25. Hot Reload의 원자성** — 파일 수정 시간 감시, 새 리소스를 임시로 만든 뒤 모두 성공할 때만 교체하는 commit 방식, 컴파일 실패 시 이전 리소스를 유지하는 방법을 학습한다.
- **S26. 조건부 컴파일과 빌드 구성** — `#ifdef`, Debug/Release 전처리기 정의, 개발 편의 기능과 배포 성능·안정성의 분리를 학습한다.

### 구현 확인 지점

- `Graphics/Shader.cpp`: HLSL → `ID3DBlob` → D3D11 Shader/Input Layout 생성과 선택적 `.cso` 저장을 구현한다.
- `Graphics/ShaderManager.cpp`: `std::filesystem::last_write_time`으로 변경을 감시하고 Reload를 요청한다.
- `Core/stdafx.h`, `Core/Game.cpp`, `Core/Graphics.cpp`: `HOT_RELOAD_ENABLED` 조건에서 등록·갱신을 활성화한다.

---

## 학생 제출 전 공통 체크리스트

- [ ] 각 과제의 프롬프트를 그대로 이해한 뒤, 구현 단위를 클래스와 책임으로 먼저 나눴다.
- [ ] 해당 과제의 선행학습 번호를 모두 실습하거나 핵심 개념을 설명할 수 있다.
- [ ] D3D11 API 호출은 `HRESULT`를 검사하고 실패 경로를 작성했다.
- [ ] GPU 리소스는 `ComPtr`, 엔진 소유 객체는 `unique_ptr`로 수명 규칙을 명확히 했다.
- [ ] Update/Render 중 컨테이너를 직접 변경하지 않고 프레임 경계를 기준으로 변경을 적용했다.
- [ ] 저장 파일을 사람이 읽을 수 있는 JSON으로 출력하고, 저장 후 다시 로드해 계층·Transform·SpriteRenderer 정보를 검증했다.
- [ ] 셰이더 컴파일 또는 Hot Reload 실패 시 화면이 깨지지 않고 이전 정상 셰이더를 유지하는지 확인했다.

## 권장 평가 방식

1. **개념 설명(30%)**: 해당 과제의 스터디 번호에서 핵심 개념을 본인 말로 설명한다.
2. **기능 시연(40%)**: 화면 출력, 입력, 스프라이트, 계층 이동, 저장/로드, 셰이더 수정 반영을 순서대로 시연한다.
3. **코드 구조(20%)**: 소유권, 오류 처리, 책임 분리가 적절한지 확인한다.
4. **검증 기록(10%)**: 실행 화면, 저장 JSON, 오류 상황과 대응을 간단히 기록한다.
