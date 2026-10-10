# Scene과 Render World 경계 설계 결정사항

`engine/scene`과 `renderer/world`의 책임과, 둘 사이에서 데이터를 넘기는 방법을 정한다.

코드와의 불일치가 있다면 문서를 고치는 것을 우선한다.

---

## 1. 문제

저장소에 두 스텁이 있고 책임이 겹친다.

```text
src/nxt/engine/scene/scene.hpp    "씬 그래프 노드와 컴포넌트 관리"
src/nxt/renderer/world/world.hpp  "렌더월드와 씬 컬렉션 관리"
```

둘 다 "씬의 뭔가를 관리한다"고 적혀 있고, 어느 쪽이 무엇을 소유하는지 정해져 있지 않다.
둘을 합치면 게임 로직과 렌더링이 한 모듈에 섞이고, 분리하면서 경계를 잘못 잡으면
렌더러가 게임 상태까지 알게 된다.

## 2. 결정

**Unreal Engine의 `UWorld → SceneProxy → FScene` 구조를 따른다.**

| 게임 스레드 (Engine 모듈) | 단방향 복사 | 렌더 스레드 (Renderer 모듈) |
|---|---|---|
| `UWorld` | | `FScene` |
| `UPrimitiveComponent` | → `SceneProxy` → | `FPrimitiveSceneInfo` |
| `ULocalPlayer` | | `FSceneViewState` |

NXT에 옮기면:

```text
engine/scene     게임 로직 · 트리 구조 · 컴포넌트 · 변환 행렬 — 논리적 소유
      │
      │  SceneProxy — 등록 시 1회 복사, 단방향
      ↓
renderer/world   GPU 핸들 · 컬링 결과 · 그릴 대상 목록 — 렌더링 소유
      │
      ↓
graphics         RHI · 커맨드 버퍼 · 동기화
```

## 3. 두 자료구조의 책임

### `engine/scene` — 논리적 씬

```text
소유한다 : 오브젝트 트리, 컴포넌트, 변환 행렬, 계층 관계
답한다   : "무엇이 존재하는가", "어디에 있는가"
```

게임 규칙에 따른다. 렌더링에 필요한지 여부는 판단하지 않는다.

### `renderer/world` — 렌더링 씬

```text
소유한다 : GPU 리소스 핸들, 드로우 목록, 프록시, 컬링 결과
답한다   : "이번 프레임에 무엇을 그릴 것인가"
```

GPU에 무언가를 올리는 책임은 여기만 진다. 삼각형 개수, 드로우 콜 순서,
CPU에서 GPU로 가는 모든 리소스를 아는 것은 `renderer/world`다.

### 경계의 규칙

- `engine/scene`은 `nxt_graphics`를 **모른다**
- `renderer/world`는 `engine/scene`을 **모른다**
- 둘은 `SceneProxy`로만 대화한다
- `SceneProxy`는 `nxt_renderer` 안에 있다. 양쪽 중 어느 쪽의 타입도 아니다

## 4. SceneProxy — 단방향 복사 브리지

프록시는 필터도 집합도 아니다. **개별 물체 단위**다.

```text
engine                                       renderer
──────                                       ────────
MeshComponent ──createProxy()──▶ SceneProxy
    │                              │
    │ 이후 게임 스레드가              │ 이후 렌더 스레드가
    │ 변경하는 것은 컴포넌트          │ 변경하는 것은 프록시
    ▼                              ▼
MeshComponent (수정됨)          SceneProxy (수정됨)
```

`createProxy()`는 **등록 시점에 한 번** 호출된다. 그 순간 컴포넌트 상태를
프록시로 **복사**한다. 이후 양쪽은 서로의 메모리를 공유하지 않는다.

이것이 핵심이다. 공유하지 않으므로 잠금이 필요 없고, 게임 스레드가 게임을
돌리는 동안 렌더 스레드가 렌더링을 하는 것이 자연스럽다.

### 복사 단위는 작게

**프록시는 필터나 배치 단위로 만들지 않는다.** 물체 하나당 하나다.

큰 단위로 묶으면 "물체 하나가 움직였는데 배치 전체를 다시 복사"가 된다.
작은 단위는 복사 비용이 조금 늘지만 변경 전파가 국소적으로 끝난다.

### 제거

```text
destroy() ──▶ 프록시 제거 요청 ──▶ 지연 목록 ──▶ fence 대기 후 실제 제거
```

제거도 복사와 같은 규칙을 따른다. GPU가 쓰고 있을 수 있으므로 즉시 없애지 않는다
(자세한 절차는 [`frame.md` §9](frame.md)).

## 5. 매 프레임 임시 객체 — FrameContext

지속 상태와 프레임 임시 상태를 분리한다.

```text
World          지속. GPU 핸들, 프록시, 메시 — 프레임이 넘어도 산다
FrameContext   임시. 매 프레임 생성 → submit 후 폐기
```

`FrameContext`가 들는 것:

```text
카메라 상태          뷰 / 투영 행렬
컬링 결과           보이는 물체 목록
정렬 결과           드로우 콜 순서
프레임 임시 데이터   upload 대상 버퍼, 임시 배열
```

**이 구분이 없으면 프레임 임시 데이터가 어디에 사는지 애매해진다.** 그리고 나중에
렌더 스레드로 옮길 때 "이건 살아남는 거야 매 프레임 새로 만드는 거야"라는 질문이
전부 여기서 답해진다.

## 6. 렌더 스레드는 나중에

**Phase 0은 단일 스레드다. 렌더 스레드를 지금 넣지 않는다.**

삼각형 하나를 그리는 데 렌더 스레드는 필요 없다. 두 개를 넣으면 큐, 동기화,
스레드 수명을 관리해야 하는데 그 대비 얻는 것이 없다.

### 그래도 지금 경계를 잡는 이유

> **프록시 패턴은 스레드가 아니라 데이터 소유에 관한 패턴이다.**

단일 스레드로 구현해도 프록시 경계는 정확히 같은 의미로 동작한다. 공유하지 않고
복사하는 구조는 스레드 개수와 무관하다.

```text
단일 스레드 + 프록시 경계   →  나중에 스레드를 빼도 경계는 그대로 유효
World 하나만 두고 나중에 분리 →  그때 전부 다시 뜯어야 함
```

따라서 **데이터 구조는 지금 분리하고, 스레드는 나중에 추가한다.** 순서를 뒤집으면
나중의 재작업을 피할 수 없다.

### 렌더 스레드를 넣게 되는 시점

하나라도 해당하면 고려한다.

- 프레임마다 같은 물체 목록을 두 번 순회하게 된다 (게임 + 렌더)
- 에셋 로딩이 GPU 큐 구분을 요구한다
- 프레임 시간이 프레임 예산을 구조적으로 초과한다 (측정으로 확인)

모듈 배치도 이 구조에 맞춘다. Unreal의 `RenderCore`가 `nxt_graphics`에 해당한다.

```text
Engine 의존:      RenderCore
Renderer 의존:     Engine, RenderCore
RenderCore 의존:   없음
```

## 7. 애셋 수명

이 절은 [`phase2.md`](../roadmap/phase2.md)의 2-A/2-C와 함께 읽는다.

### 핸들을 보관한다. 포인터를 보관하지 않는다

세 엔진이 같은 답을 준다.

```text
BAD   MeshPtr mesh_;      참조 카운트가 0이 되지 않는다
GOOD  MeshID meshId_;     필요할 때만 resolve
```

`nxt_core::handle::Handle<Tag>`를 쓴다. 이미 세대(generation)를 지원하므로
해제된 핸들을 구분할 수 있다.

### 매니저가 소유한다

```text
nxt_assets/asset_manager.hpp   CPU 측 파싱 데이터
nxt_renderer/world             GPU 리소스 핸들
```

**`assets`는 GPU 리소스를 만들지 않는다. `renderer/world`는 파일을 읽지 않는다.**
파싱된 데이터와 올라간 리소리가 서로를 모르게 하는 것이 수명 버그를 막는다.

### 두 단계 수명

```text
SharedAssetManager   앱 전체 수명 — 공용 머티리얼, UI 폰트
LocalAssetManager    부모 씬 수명 — 레벨 메시, 레벨 텍스처
```

### refcounting은 넣지 않는다

Phase 2는 **매니저 소유 + 명시적 `unload()`** 다.

refcounting을 넣을 시점은 **같은 리소스를 서로 다른 씬이 공유할 때**다.
지금 요구되지 않은 공유를 대비해 참조 카운터를 두는 것은
`CODING_RULES.md` §10이 금지하는 "요구사항이 없는 추상화"다.

## 8. 핵심 원칙

1. `engine/scene`은 논리적 씬만 소유한다. GPU를 모른다.
2. `renderer/world`는 렌더링 씬만 소유한다. 게임 규칙을 모른다.
3. 둘은 `SceneProxy`로만 대화하며, 복사는 단방향이다.
4. 프록시는 물체 단위로 만든다. 배치 단위로 묶지 않는다.
5. `FrameContext`는 매 프레임 새로 만든다. `World`만 지속된다.
6. 프록시 경계는 지금 잡고, 렌더 스레드는 나중에 추가한다.
7. 애셋은 핸들로 보관한다. 포인터 멤버를 두지 않는다.
8. refcounting은 공유 요구가 실제로 생길 때 도입한다.