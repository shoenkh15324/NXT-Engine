# Phase 1 — 10만 삼각형

> **목표**: 삼각형 10만 개가 도는 씬을 만들고, **무엇이 느린지 숫자로 답한다.**
>
> 이 Phase의 산출물은 빠른 렌더러가 아니라 **병목을 판단하는 절차와 기준선**이다.

Phase 0는 "이 아키텍처로 그려진다"를 증명했다. Phase 1은 "이 아키텍처가 규모를
견디는가"를 묻는다. 그 질문에 답하려면 먼저 **재는 도구**가 필요하고, 그 다음에야
최적화가 필요하다.

**상태**: 착수 전 (Phase 0 완료 — 삼각형 · 계층 배선 · 리사이즈 검증까지)

---

## 0. 한눈에 보기

| 마일스톤 | 답하려는 질문 | 산출물 | 다음 단계에 주는 것 |
|---|---|---|---|
| **1-A 기준선** | 병목이 CPU인가 GPU인가? | 10만 오브젝트 + 계측 + 기준선 리포트 | 1-B를 어디까지 할지 |
| **1-B CPU 런타임** | CPU 병목을 병렬로 줄일 수 있나? | Scheduler · Worker · Job · parallel_for | Phase 2 비동기 로딩의 실행 기반 |
| **1-C 이벤트** | 런타임 위에서 이벤트를 안전하게 흘릴 수 있나? | Event · MPSC 큐(다중 생산자·단일 소비자) · Dispatcher | Phase 2 로딩 완료 통지 |
| **1-D 벤치마크** | 이 숫자가 재현되는가? | CLI 리포트 + 기록 | Phase 2가 비교할 기준선 |

```text
1-A ──▶ [게이트 A] ──▶ 1-B ──▶ [게이트 B] ──▶ 1-C ──▶ [게이트 C] ──▶ 1-D
 측정      CPU 병목?     병렬화     이득 있음?     이벤트      재현 가능?
```

**게이트를 통과하지 못하면 다음 마일스톤으로 넘어가지 않는다.** 각 게이트에서
"측정 결과가 다음 단계를 정당화하는가"를 문서로 남긴다. 이 규칙이 이 Phase의
핵심이다 — 로드맵은 할 일 목록이 아니라 **판단의 순서**다.

---

## 1. 왜 이 Phase인가

### 1.1 Phase 0가 답하지 못한 것

Phase 0는 깊이 테스트가 걸린 삼각형 두 개가 화면에 나오는 것까지 확인했다. 이
숫자는 **정확성**의 증거이지 **확장성**의 증거가 아니다.

```text
삼각형 2개   : 무엇이 느린지 알 수 없다. 전부 빠르다
삼각형 10만 개: 처음으로 "왜 안 빨라지지?"가 실제 질문이 된다
```

10만 개는 특별한 숫자가 아니다. 다만 **CPU 작업량이 프레임 예산을 넘기 시작할 수
있는 규모**이고, 그때부터 "렌더링이 느린 것인지, CPU가 느린 것인지, GPU가 느린
것인지"를 구분해야 한다. 구분하지 못하면 최적화는 추측이 된다.

### 1.2 왜 CPU 런타임을 여기서 넣는가

CPU 런타임(잡 시스템)을 Phase 0에 넣지 않은 이유는 **소비자가 없었기** 때문이다.
삼각형 두 개에는 스레드 풀이 필요 없다. 필요 없는 추상화를 먼저 넣으면
`CODING_RULES.md` §16이 금지하는 "측정 없는 최적화"가 된다.

여기서는 순서가 뒤집힌다.

```text
1-A : 10만 개를 단일 스레드로 그린다
      → "CPU가 병목이다"라는 측정 결과가 나온다
      → 그 결과가 1-B의 근거가 된다
```

즉 **1-A의 결과가 1-B의 범위를 정한다.** 1-A에서 GPU가 병목으로 나오면 1-B의
렌더링 적용은 보류하고, 인프라만 만든 뒤 Phase 2의 비동기 로딩에서 쓴다. 이
판단을 게이트 A에서 내린다.

### 1.3 이 Phase가 증명할 것

1. 10만 오브젝트의 프레임 시간을 **구성 요소별로 설명**할 수 있다.
2. 병렬화가 실제로 그 숫자를 바꾼다. (또는 바꾸지 못한다는 것을 안다.)
3. 이벤트 경로가 **유실·중복 없이** 닫힌다.
4. 같은 입력에서 같은 결론이 나온다. (재현성)

---

## 2. 설계 원칙

이 Phase의 모든 결정은 아래 다섯 줄로 설명된다.

| 원칙 | 이 Phase에서의 의미 | 어기면 생기는 일 |
|---|---|---|
| **측정 → 최적화** (§16) | 1-A 없이 1-B를 시작하지 않는다 | 무엇이 빨라졌는지 모른 채 복잡도만 늘어난다 |
| **계약 먼저** (§17) | 생산자·소비자·동기화·수명을 코드보다 먼저 정한다 | 재현되지 않는 동시성 버그 |
| **소유권 명시** (§9) | Job·Event·World의 owner가 코드에 보인다 | use-after-free, 이중 해제 |
| **요구 없는 추상화 금지** (§10) | lock-free·work stealing·parallel_invoke는 측정 후 | 유지비만 늘고 삭제도 어렵다 |
| **경계 유지** | `core`는 `graphics`를 모른다. 이벤트는 렌더링을 모른다 | 계층이 무너지고 Phase 3에서 전부 뜯는다 |

표의 `§번호`는 [`CODING_RULES.md`](../CODING_RULES.md)의 절 번호다.

특히 **2번과 4번은 서로 당긴다.** "계약을 먼저 정하라"와 "추상화를 만들지
마라"가 충돌할 때의 기준은 하나다 — **계약은 지금 필요한 만큼만, 구현은 가장
단순하게.** 예를 들어 MPSC 큐는 "다중 생산자·단일 소비자"라는 계약만 먼저
정하고, 첫 구현은 mutex로 둔다(§5.5).

---

## 3. 전체 구조

### 3.1 프레임 안의 데이터 흐름

```text
[World: 10만 오브젝트]
   │  변환 행렬 · AABB
   ▼
[프레임 컬링]                 ← 1-B에서 이 단계만 병렬화한다
   │  보이는 오브젝트
   ▼
[DrawList]
   │
   ▼
[Renderer::record] ──▶ [CommandEncoder] ──▶ submit ──▶ GPU ──▶ present
   │                        │
   └──── 계측 지점 ──────────┘   (1-A2: CPU 구간 · GPU timestamp)
```

병렬화하는 대상은 **컬링**이다. `record`와 `submit`은 프레임 루프 한 스레드가
그대로 한다. 이유는 두 가지다.

- 렌더 스레드를 지금 넣지 않는다 ([`scene_world.md`](../design/scene_world.md) §6).
- draw call 순서는 결정적이어야 벤치마크가 재현된다.

### 3.2 모듈 배치

```text
nxt_core
  time/            (기존) 프레임 계측의 토대
  jobs/            (신규) Job · JobCounter · Scheduler · Worker
  concurrency/     (신규) SPSC(단일 생산자·단일 소비자, 작업 큐) · MPSC(이벤트 큐)
  event/           (신규) Event · EventQueue · Dispatcher
  diagnostics/     (신규) Profiler

nxt_renderer
  world/           (신규) 지속 상태: 오브젝트 · GPU 핸들
  draw_list.hpp    (신규) 프레임 draw 목록 — 컬링 결과
  frame_context.*  (확장) 프레임 임시 상태: 카메라 · 컬링 결과
  passes/          (확장) push constant로 오브젝트 변환 전달

nxt_engine
  frame_loop/      (확장) 프레임 계측 훅
  engine/          (확장) 벤치마크 모드 · 종료 순서

apps/sandbox        CLI 옵션 · 리포트 출력
```

> **주의 — `engine/scene`은 이 Phase에서 건드리지 않는다.**
> 저장소에는 `renderer/world`("씬 컬렉션")와 `engine/scene`("씬 그래프")이 함께
> 있고 책임이 겹친다. Phase 1은 `renderer/world`만 쓴다. 두 경계는 Phase 2 착수
> 전에 ADR로 확정한다 ([`scene_world.md`](../design/scene_world.md) §1–§2).
> 지금 `engine/scene`을 채우면 Phase 2에서 같은 결정을 두 번 하게 된다.

---

## 4. 마일스톤

> 체크박스는 **완료 기준을 만족했을 때만** 체크한다. 코드를 만들었다는 이유로
> 체크하지 않는다. 게이트 판정은 체크와 별도로 문서에 기록한다.

### 1-A — 기준선: 병렬 없이 측정한다

> **왜 필요한가**
> 병렬화하기 전에 "지금 무엇이 느린가"를 알아야 한다. 기준선 없이 스레드를 넣으면
> 빨라졌는지조차 판단할 수 없고, 나중에 "예전엔 어땠지?"를 다시 측정해야 한다.
>
> **이 단계는 일부러 최적화하지 않는다.** 컬링도 단일 스레드, draw도 오브젝트당
> 한 번. 기준선을 흐리면 이후 비교가 무의미해진다.

**설계 결정**

| 결정 | 선택 | 이유 |
|---|---|---|
| 오브젝트 표현 | 단위 삼각형 하나 + 오브젝트별 변환·색 | 메시 10만 개를 업로드하면 측정 대상이 메모리 대역폭으로 바뀐다 |
| 변환 전달 | push constant (mat4 + color) — draw마다 셰이더에 넘기는 작은 상수 블록 | 오브젝트당 1 draw를 유지하면서 RHI 추가를 최소화한다 |
| draw 방식 | 오브젝트당 1 draw call | 이 Phase가 재는 것은 **CPU 기록·제출 경로**다 (§5.2) |
| 배치 | 고정 시드 그리드/랜덤. 일부는 프러스텀 밖에 둔다 | 같은 입력에서 같은 draw 순서가 나와야 하고, 컬링 효과가 0이 되면 안 된다 |
| 카메라 | 고정 | 카메라 애니메이션은 변수를 늘린다. 벤치마크는 변수를 줄인다 |
| 컬링 | 오브젝트 AABB(축 정렬 경계 상자) vs 프러스텀 | CPU 작업량이 선형으로 늘고, 청크로 나누기 쉬워 병렬화 효과를 분리해 볼 수 있다 |

**서브스텝**

- [x] **1-A1** — `renderer/world`에 N개 오브젝트(변환·AABB), push constant 경로
  - 완료 기준: `--objects N`을 바꾸면 draw call 수가 N과 일치한다
- [x] **1-A2** — `FrameStats` · `Profiler` scope 계측 · GPU timestamp query
  - 완료 기준: CPU 프레임과 GPU 프레임이 **따로** 집계된다
- [ ] **1-A3** — 프러스텀 AABB 컬링 + `DrawList`
  - 완료 기준: 컬링 전/후 draw call 수가 함께 찍힌다
- [ ] **1-A4** — 기준선 리포트(워밍업·측정 분리, 중앙값·편차)
  - 완료 기준: 같은 설정 3회 실행의 편차가 기록된다

**왜 이 순서인가**

- A1이 없으면 잴 대상이 없다. A2가 없으면 A3의 효과를 숫자로 말할 수 없다.
- A3(컬링)을 기준선에 넣는 이유: 1-B에서 병렬화할 대상이 바로 이 단계다.
  단일 스레드 버전을 먼저 만들어 두어야 "같은 일을 병렬로" 비교할 수 있다.
- A4가 없으면 그 숫자가 한 번의 우연인지 알 수 없다.

1-A4의 리포트는 1-D와 같은 형식을 쓴다. `workers: 1`, `culling: off/on`으로
비교하면 병렬화 전후를 같은 자로 볼 수 있다.

**게이트 A — 1-B의 범위를 정한다**

| 관측 | 해석 | 다음 행동 |
|---|---|---|
| `cpu frame`이 프레임 예산을 넘고 `gpu frame`이 그보다 작다 | CPU-bound | 1-B 전체 진행. 병렬 컬링의 효과를 주장할 수 있다 |
| `gpu frame`이 `cpu frame`보다 크거나 `fence wait`가 큼 | GPU-bound | 1-B는 **인프라만** 만든다. 렌더링 적용(1-B4)은 보류하고, 배칭·인스턴싱을 Phase 3 후보로 기록 |
| 둘 다 여유, fps만 낮음 | present/vsync 또는 측정 문제 | present mode와 계측 지점을 먼저 확인한다 |
| 실행마다 편차가 큼 | 측정 절차 문제 | 1-A4를 다시 한다. 이 상태의 숫자는 기준선이 아니다 |

> GPU-bound로 판정돼도 1-B를 **전부 건너뛰지는 않는다.** Phase 2의 비동기 로딩이
> 이 런타임을 전제로 하기 때문이다. 이 경우 1-B1~1-B3과 1-B5만 하고, 1-B4(병렬
> 컬링 적용)는 생략한다. "렌더링이 빨라졌다"는 주장도 하지 않는다.

---

### 1-B — CPU 런타임: 측정된 병목을 병렬로 줄인다

> **왜 필요한가**
> 1-A가 CPU 병목을 확인했을 때, 병렬화는 가장 직접적인 해법이다. 동시에 Phase 2의
> 비동기 로딩이 쓸 실행 기반이기도 하다. 잡 시스템은 공짜가 아니다 — 스레드 수명,
> 큐, 동기화, 디버깅 비용이 붙는다. 그래서 **렌더링에 적용하는 것은 측정된 병목일
> 때만** 한다. 인프라 자체는 Phase 2의 전제이므로 만든다.
>
> **이 단계가 증명하는 것**
> CPU 병목일 때 병렬화가 실제로 프레임 시간을 줄이고, `wait_idle()`이 진짜
> 동기점을 보장한다.

**설계 결정**

- **`thread_source` 인터페이스 + `std_thread_source` 구현.**
  이유: 테스트에서 가짜 스레드 소스를 넣어 결정적 실행을 만들 수 있고, 나중에
  플랫폼 스레드로 교체할 여지를 남긴다. `core`는 `platform`에 의존할 수 없으므로
  기본 구현은 `std::thread`다. (OS API를 직접 부르지 않는 것은 결과적으로 얻는
  이점이지, `platform` 의존을 만들라는 뜻이 아니다.)

- **Worker 수 = `std::thread::hardware_concurrency()`.**
  `-1`을 하지 않는다. 메인 스레드는 `wait_idle()`에서 블록되고 잡을 처리하지
  않기 때문이다. TBB와 EnkiTS가 코어 수를 그대로 쓰는 이유와 같다.
  `hardware_concurrency()`가 0을 돌려주면 1로 보정한다.

- **작업 큐는 worker별 SPSC, 제출은 라운드 로빈.**
  pop 경로에 경합이 없고, work stealing 없이도 균등한 청크에는 충분하다.
  **Phase 1의 계약: 잡 제출은 메인 스레드만 한다.** 워커가 다시 잡을 제출하면
  SPSC가 깨지므로, 그 요구가 생기는 시점(Phase 2 이후)에 MPSC 작업 큐나 stealing을
  검토한다. 이 제약을 문서와 `static_assert`/assert로 남긴다.

- **`JobCounter`가 유일한 대기 프리미티브.**
  `std::future`를 잡마다 만들지 않는다. future는 잡 하나당 할당이 붙고 "N개를
  기다린다"를 조합하기 어렵다. 카운터는 atomic 하나로 N개 완료를 기다린다.
  소유권: 카운터는 **호출자**가 소유한다. 스케줄러는 포인터만 빌리고, `wait()`가
  끝나기 전에 파괴하지 않는다. 이 계약을 주석과 assert로 고정한다.

- **`Job`은 `std::function` + 카운터 포인터로 시작한다.**
  10만 개 잡을 만들지 않는다. `parallel_for`는 `workers × 4` 정도의 청크만 만든다.
  따라서 잡 하나의 `std::function` 할당은 측정 가능한 수준이 아니다. 필요하면
  나중에 인라인 저장소로 바꾼다 — 지금은 §10에 따라 단순한 쪽을 택한다.

- **`wait_idle()`의 계약: 호출 이전에 제출된 모든 잡이 끝나야 반환한다.**
  이 보장이 없으면 `record`가 아직 끝나지 않은 컬링 결과를 읽는다. 구현은 카운터가
  0이 될 때까지 대기하며, 워커는 짧게 spin한 뒤 condition variable로 잠든다.
  wake 지연이 문제로 측정되면 그때 조정한다.

- **work stealing은 만들지 않는다.**
  worker별 SPSC + 라운드 로빈의 불균형이 측정되지 않았다. `work_stealing_deque.hpp`
  스텁은 삭제한다. worker utilization이 낮게 나오면 1-D에서 Phase 2 후보로 기록한다.

- **`parallel_invoke`는 만들지 않는다.**
  Phase 1에는 독립적인 두 작업이 없다. 컬링은 `parallel_for` 하나로 충분하다.
  독립 작업이 실제로 생기는 Phase 2에서 추가한다.

**서브스텝**

- [ ] **1-B1** — `thread_source` · `std_thread_source` · worker 수 결정
  - 완료 기준: 가짜 소스로 생성/종료 테스트가 통과한다
- [ ] **1-B2** — `Job` · `JobCounter`
  - 완료 기준: N개 잡 제출 후 `wait()`가 정확히 N개 완료를 기다린다
- [ ] **1-B3** — `Scheduler` · `WorkerPool` · `Worker` · worker별 SPSC 큐
  - 완료 기준: submit → 실행 → idle → 종료 경로가 닫힌다
- [ ] **1-B4** — `parallel_for` + 병렬 컬링
  - 완료 기준: 같은 워크로드에서 1-A보다 `cull` 시간이 줄어든다
- [ ] **1-B5** — 동시성 검증
  - 완료 기준: 반복 스트레스 100회 실패 0, 가능한 환경에서 TSan 0

**왜 이 순서인가**

- B1~B3은 아래에서 위로 쌓는다. 스레드가 없으면 잡을 실행할 곳이 없고, 잡이
  없으면 스케줄러가 배분할 것이 없다.
- B4는 B2~B3의 단위 테스트가 통과한 뒤에 한다. 스케줄러가 틀린 상태에서 렌더링에
  적용하면 동시성 버그와 렌더링 버그를 동시에 디버깅하게 된다.
- B5는 B4까지 합친 경로를 반복 실행해 계약을 확인한다. 단위 테스트가 통과해도
  프레임 경계에서만 드러나는 문제가 있다.

> **TSan에 대해.** MSVC에는 ThreadSanitizer가 없다. WSL·Linux CI·별도 Linux 머신 중
> 실제로 돌아가는 경로를 하나 정해 1-B5의 기준으로 삼는다. 그 경로가 없으면 "TSan
> 통과"를 주장하지 않는다. 대신 (1) 가짜 스레드 소스로 결정적 실행, (2) 반복
> 스트레스, (3) 카운터·큐 불변식 assert로 대체하고, **검증의 한계를 문서에 남긴다.**

**설계 노트 — 왜 `WorkerPool`과 `Scheduler`를 나누는가**

- `WorkerPool`: 스레드의 **수명**을 소유한다. 생성·시작·정지·join.
- `Scheduler`: **제출 정책**을 소유한다. 큐 선택, 청크 분배, `wait_idle()`.

둘을 합치면 "스레드를 띄우는 일"과 "일을 나누는 일"이 한 타입에 섞인다. 지금은
책임이 둘이므로 나누되, 타입을 더 늘리지는 않는다. (별도 `JobSystem` 파사드는
만들지 않는다 — 소비자가 하나뿐이다.)

**게이트 B — 이벤트를 얹을 것인가**

| 관측 | 해석 | 다음 행동 |
|---|---|---|
| 병렬 컬링으로 `cpu frame`이 유의미하게 줄었다 | 런타임이 작동한다 | 1-C 진행 |
| 줄지 않았거나 느려졌다 | 동기화·분배 비용이 이득을 상쇄한다 | 원인을 기록한다. 1-C는 진행하되 런타임 추가 최적화는 하지 않는다 |
| 스트레스 실패(또는 TSan 검출) | 계약 위반 | 1-C 금지. 1-B를 먼저 고친다 |
| worker utilization이 낮다 | 분배 불균형 | work stealing을 Phase 2 후보로 기록 |

---

### 1-C — 이벤트: 런타임 위에서 안전하게 흘린다

> **왜 필요한가**
> Phase 2의 비동기 로딩은 "다 읽었다"를 어딘가에 알려야 한다. 그 통지 경로를
> 렌더링 코드와 분리해 여기서 먼저 검증한다. 이벤트는 나중에 입력·씬 전환·에셋
> 완료까지 확장되지만, **Phase 1에서는 경로의 안전성만** 증명한다.
>
> **이 단계가 증명하는 것**
> 다중 생산자(메인 + 워커) → 단일 소비자(디스패처) 경로가 유실·중복 없이 닫힌다.

**설계 결정**

- **`EventTypeId = std::uint32_t`, RTTI 없음.**
  `typeid`와 가상 함수는 hot path에 비용과 불확실성을 만든다. 정수 ID는 비교가
  싸고, 큐에 그대로 담을 수 있으며, 직렬화에도 유리하다.
  Phase 1은 `event.hpp`에 명시적 상수로 ID를 정의하고 `static_assert`로 중복을
  막는다. 타입 자동 등록(전역 레지스트리)은 만들지 않는다 — 초기화 순서 문제와
  숨은 전역 상태를 만든다.

- **Payload는 고정 크기 POD(64B), 초과하면 핸들로 보낸다.**
  큐에 넣는 순간 복사되므로 생산자의 스택을 가리키는 포인터를 담을 수 없다.
  이 규칙이 이벤트 수명 버그의 대부분을 막는다. 큰 데이터(메시 정점 등)는
  이벤트에 담지 않고 `Handle`만 담는다. `static_assert(std::is_trivially_copyable_v<Event>)`.

- **큐 계약은 MPSC, 첫 구현은 mutex.**
  생산자는 메인 + 워커 여럿, 소비자는 하나다. 이 계약은 지금 확정한다.
  하지만 첫 구현을 lock-free로 만들 이유는 아직 없다. `std::mutex` + `std::deque`는
  정확하고 단순하며, 이벤트 발행량이 적다. 경합이나 할당이 병목으로 확인되면
  **API를 바꾸지 않고 내부만** 고정 용량 링 버퍼나 lock-free 링 버퍼로 교체한다.
  (초기 계획은 MPSC를 lock-free로 전제했지만, §16에 맞춰 구현 선택을 측정 뒤로
  미룬다.)

- **소비자는 메인 스레드 하나.**
  `wait_idle()` 직후, 프레임 경계에서 큐를 비우고 핸들러를 호출한다.
  워커가 직접 핸들러를 부르면 소비자가 여럿이 되어 단일 소비자 계약이 깨지고,
  핸들러가 프레임 상태를 만질 때 동기화가 필요해진다. Phase 1의 핸들러는
  가볍게 유지한다(다음 단계에 일을 넘기는 정도). 디스패치 시간이 프레임 병목으로
  측정되면 그때 전용 소비자 스레드로 옮긴다.

- **순서 계약: 같은 생산자가 넣은 이벤트의 순서는 유지된다.**
  서로 다른 생산자 사이의 순서는 보장하지 않는다. 이 문장을 주석에 박는다.
  보장하지 않는 것을 테스트가 기대하면 테스트가 거짓말이 된다.

- **`Profiler`는 런타임 토글.**
  릴리스에서 컴파일 아웃하지 않는다. 컴파일 아웃하면 "오버헤드 측정" 자체가
  불가능해진다. 대신 비활성 경로를 atomic load 하나로 두고, 그 비용을 1-C5에서
  잰다. 샘플은 링 버퍼에 모아 프레임마다 로그를 찍지 않는다 — I/O가 측정을
  방해하기 때문이다.

**서브스텝**

- [ ] **1-C1** — `Event` · `EventTypeId` · payload 계약
  - 완료 기준: trivially copyable, ID 중복 0, 64B 초과 시 assert
- [ ] **1-C2** — `MpscQueue` (mutex 구현)
  - 완료 기준: N 생산자 → 1 소비자 스트레스에서 유실·중복 0
- [ ] **1-C3** — `Dispatcher` 핸들러 등록/호출
  - 완료 기준: 미등록 타입은 명시적으로 거부되고 조용히 사라지지 않는다
- [ ] **1-C4** — `Event → Queue → Dispatcher → Handler` 통합
  - 완료 기준: 프레임 경계에서 처리 건수가 정확히 맞는다
- [ ] **1-C5** — `Profiler` ON/OFF 오버헤드 측정 + 리포트 통합 (도구는 1-A2에서 만든다)
  - 완료 기준: 오버헤드 수치가 리포트에 들어간다

**왜 이 순서인가**

- C1(계약)이 먼저다. 이벤트 크기와 수명 규칙을 정하지 않으면 큐를 만들 수 없다.
- C2(큐)를 C3(디스패처)보다 먼저 둔다. 큐의 유실·중복을 먼저 배제해야 디스패처
  버그와 구분할 수 있다.
- C4(통합)에서 처음으로 생산자와 소비자가 만난다. C5(오버헤드)는 경로가 닫힌
  뒤에야 의미가 있다.

**게이트 C — Phase 2로 넘어갈 조건**

| 관측 | 해석 | 다음 행동 |
|---|---|---|
| 유실·중복 0, 스트레스 통과(가능하면 TSan) | 경로가 닫혔다 | 1-D 진행 |
| 디스패치가 프레임 예산을 잡아먹는다 | 핸들러가 무겁다 | 핸들러를 가볍게 만들거나 전용 소비자 검토 |
| 큐 경합이 병목 | mutex 구현의 한계 | lock-free 교체를 Phase 2 후보로 기록 (이 Phase에서 하지 않음) |
| profiler 오버헤드가 크다 | 계측이 대상을 바꾼다 | 기본 OFF, 벤치마크에서만 ON으로 확정 |

---

### 1-D — 벤치마크: 숫자를 재현 가능하게 만든다

> **왜 필요한가**
> 한 번의 측정은 숫자가 아니다. 같은 입력에서 같은 결론이 나와야 Phase 2가
> 기준선으로 쓸 수 있다. 이 단계가 없으면 Phase 2는 "예전보다 빠른 것 같다"는
> 인상으로 진행하게 된다.
>
> **이 단계가 증명하는 것**
> 이 Phase의 결론이 머신·실행·설정에 대해 재현된다.

**설계 결정**

| 결정 | 선택 | 이유 |
|---|---|---|
| 빌드 | Release 또는 RelWithDebInfo | Debug 숫자는 성능이 아니다. 검증은 Debug에서 따로 한다 |
| 워밍업 | 앞 60프레임 제외 | 파이프라인·버퍼 생성·드라이버 준비 비용을 뺀다 |
| 통계 | 중앙값 + p95, 3회 실행 | 평균은 간헐적 끊김(hitch)을 숨긴다 |
| present | 기본 FIFO, 필요하면 벤치마크에서만 상한 해제 | VSync가 프레임 간격의 상한을 만든다 |
| 입력 | 고정 시드 | 같은 설정에서 같은 draw 순서가 나와야 한다 |
| 기록 | 환경(CPU/GPU/빌드) + 설정 + 결과 표 | 머신이 바뀌면 숫자도 바뀐다. 환경 없는 숫자는 비교할 수 없다 |

**서브스텝**

- [ ] **1-D1** — `sandbox --benchmark` CLI (`objects`, `frames`, `workers`, `culling`, `profiler`)
  - 완료 기준: 옵션 조합이 리포트에 그대로 찍힌다
- [ ] **1-D2** — 워밍업/측정 분리, 중앙값·p95·편차, worker 수 스윕
  - 완료 기준: 같은 설정 3회 실행의 편차가 기록된다
- [ ] **1-D3** — 결과 표를 문서에 기록하고 Phase 2로 인계
  - 완료 기준: Phase 2가 비교할 기준선 표가 있다

**왜 이 순서인가**

- D1(CLI)이 없으면 같은 실험을 반복할 수 없다.
- D2(통계)가 없으면 한 번의 숫자를 결론으로 착각한다.
- D3(기록)이 없으면 Phase 2가 비교할 대상이 사라진다.

**리포트 형식**

```text
NXT CPU Runtime Benchmark

build        : Release
gpu          : <name>
present      : FIFO
workers      : 8
objects      : 100,000
culling      : on
frames       : 300 (warmup 60)

cpu frame    : xx.xx ms median (p95 xx.xx)  [cull xx.xx / record xx.xx / submit xx.xx]
gpu frame    : xx.xx ms median (p95 xx.xx)  (timestamp)
fence wait   : xx.xx ms median
draw calls   : 87,655 submitted / 12,345 culled (100,000 total)
throughput   : xxx,xxx objects/sec
worker util  : xx %
profiler     : off (on overhead: x.xx ms/frame)
```

Debug 빌드 숫자는 성능이 아니다. **판정은 Release(또는 RelWithDebInfo)에서만
한다.** Debug는 정확성·검증 레이어·assert 용도다.

**Phase 1 완료 정의**

- [ ] 10만 오브젝트가 월드에 있고, 컬링을 끄면 draw call 수가 입력과 일치한다
- [ ] CPU 프레임과 GPU 프레임이 분리 측정되고 병목이 숫자로 설명된다
- [ ] 병렬 컬링의 전/후가 같은 조건에서 비교된다
- [ ] `Event → Queue → Dispatcher → Handler`가 유실·중복 없이 닫힌다
- [ ] TSan(가능한 환경)과 반복 스트레스가 통과한다
- [ ] GPU 검증 레이어 VUID/ERROR 0건 (Debug, 검증 레이어 ON)
- [ ] profiler ON/OFF 오버헤드가 기록된다
- [ ] Phase 2가 쓸 기준선 표가 문서에 있다

---

## 5. 설계 노트 — 왜 이렇게 하는가

### 5.1 GPU 시간은 어떻게 재는가

`fence wait`만으로는 GPU 실행 시간을 알 수 없다. fence는 "제출이 끝났다"만
말하고, 큐가 present에서 막히면 그 영향까지 섞인다. 그래서 **한 프레임에
timestamp 두 개**를 찍는다.

```text
command buffer 기록
  ├─ writeTimestamp(start)
  ├─ render pass (draw N개)
  └─ writeTimestamp(end)

제출 → fence 신호 → 다음 레인에서 결과 읽기
```

설계 규칙:

- **draw마다 찍지 않는다.** 10만 개 timestamp는 측정 대상 자체를 바꾼다.
  GPU는 프레임 경계만 찍고, CPU 구간(컬링/기록/제출)은 `Profiler`가 잰다.
- **결과는 그 프레임의 fence가 신호된 뒤에 읽는다.** 제출 직후 읽으면 GPU를
  stall시킨다. 프레임 레인이 여러 개면 보통 N프레임 뒤에 읽는다.
- **timestamp를 지원하지 않는 GPU면 `fence wait` 기반 근사치로 대체**하고,
  리포트에 "approximate"라고 표시한다. 없는 기능을 가정하지 않는다.
- 변환 주기(`timestampPeriod`)는 디바이스가 준다. RHI에 **QueryPool 최소 API**를
  추가하고, 주기는 생성 시 함께 받는다. 이 Phase에서 필요한 것은 timestamp
  하나뿐이므로 occlusion query 같은 일반화는 하지 않는다.

### 5.2 왜 오브젝트당 draw call인가

이 선택은 "이 방식이 옳다"는 주장이 아니다. **CPU 경로의 상한을 재기 위한
부하**다.

- 인스턴스 버퍼를 쓰면 오브젝트 10만 개를 한 draw로 묶을 수 있다. 하지만 그러면
  CPU가 하는 일이 "인스턴스 버퍼 채우기"로 줄어, 이 Phase가 재려는 draw
  기록·제출 경로가 사라진다.
- 배칭·인스턴싱은 GPU 처리량을 높이는 최적화다. CPU 경로의 상한을 먼저 재 두면,
  나중에 "배칭이 얼마를 벌었는가"를 같은 자로 말할 수 있다.

즉 이 숫자는 **최종 렌더링 전략이 아니라 벤치마크 워크로드**다. 문서가 이 둘을
섞지 않도록 주의한다.

### 5.3 Job과 JobCounter의 수명

```text
JobCounter (호출자 소유)
   ▲
   │ counter 포인터
Job { fn, counter }
   │
   ▼
Worker가 실행 → fn() → counter.fetch_sub(1)
   │
   ▼
호출자: counter.wait() → 0이면 반환
```

- `JobCounter`는 복사할 수 없다. 호출자가 소유하고, `wait()` 전에 파괴하지 않는다.
- 워커는 잡을 실행한 뒤 반드시 카운터를 감소시킨다. 예외로 빠져나가는 경로가
  없으므로(§13) 이 불변식이 유지된다.
- `wait_idle()`은 "이 시점까지 제출된 모든 잡"을 기다린다. 이후에 제출된 잡은
  대상이 아니다. 이 구분이 없으면 대기 범위가 모호해진다.

### 5.4 Worker 수와 큐

```text
메인 스레드
   │ submit (라운드 로빈)
   ├──▶ SPSC[0] ──▶ Worker 0
   ├──▶ SPSC[1] ──▶ Worker 1
   └──▶ SPSC[2] ──▶ Worker 2
```

- **worker별 SPSC**를 쓰는 이유: pop 경로에 공유 atomic이 없다. 라운드 로빈은
  청크가 균등할 때 충분하다.
- **work stealing을 넣지 않는 이유**: 불균형이 아직 측정되지 않았다. 넣으면
  디버깅 비용과 메모리 순서 복잡도만 먼저 지불한다.
- **제출은 메인 스레드만** 한다. 워커가 제출을 시작하면 SPSC 계약이 깨진다.
  이 제약은 Phase 2에서 필요해질 수 있으므로, 그때 MPSC 작업 큐를 검토한다.
- 청크 크기는 `ceil(N / (workers × 4))`에서 시작한다. 4배는 균형과 오버헤드의
  출발점일 뿐이며, 1-D의 스윕에서 조정한다.
- 병렬 컬링은 청크별 `DrawList`에 쓴 뒤 **청크 순서대로** 합친다. 그래야 스레드
  스케줄링과 무관하게 draw 순서가 같고, 벤치마크가 재현된다.

### 5.5 Event의 수명과 소비자

```text
생산자(메인/워커)                소비자(메인)
   │ publish(Event)                │
   ├──────────────▶ MPSC 큐 ───────┤
   │  (값 복사)                    │ wait_idle()
   │                               ▼
   │                          Dispatcher
   │                          ├─ type 1 → handler
   │                          └─ type 2 → handler
```

- **값으로 복사**한다. 포인터를 담으면 생산자의 스택이 사라진 뒤 소비자가 읽는다.
- **payload 64B 제한**은 타협이 아니라 계약이다. 큰 데이터는 핸들로 보낸다.
- **소비자는 하나**다. 생산자는 큐에 넣기만 하므로, 핸들러가 프레임 상태를 만질 때
  생산자와의 동기화를 생각할 필요가 없다. 큐 쪽 동기화만 지키면 된다.
- **mutex 먼저, lock-free는 측정 후.** 계약(MPSC)과 구현(lock-free)을 분리해
  두면 나중에 교체가 쉽다. 이 분리가 이 설계의 핵심이다.

### 5.6 Profiler — 측정이 대상을 바꾸지 않게

도구 자체는 1-A2에서 최소로 만든다. 이 절은 그 위에 얹을 정책의 기준이다.

- scope 기반 RAII(`NXT_PROFILE_SCOPE("cull")`)로 시작·종료를 자동화한다.
- 샘플은 링 버퍼에 모으고, 프레임마다 로그를 찍지 않는다. 로그 I/O는 프레임
  시간에 직접 영향을 준다.
- 보고는 평균이 아니라 **중앙값과 p95**를 쓴다. average는 끊김(hitch)을 숨긴다.
- 비활성 경로는 atomic load 하나다. 그 비용을 1-C5에서 측정해 "계측 때문에
  느려진 것인지"를 구분할 수 있어야 한다.

### 5.7 Arena와 Pool은 측정 후에 쓴다

`core/memory`의 `Arena`와 `Pool`은 구현돼 있지만 아직 소비자가 없다. 이 Phase에서
처음 후보가 생긴다.

| 대상 | 도구 | 도입 조건 |
|---|---|---|
| Job 객체 | `Pool<Job>` | 잡 할당이 측정된 병목일 때 |
| 프레임 컬링 데이터 | `Arena` | 프레임 단위 임시 할당이 병목일 때 |

**둘 다 조건부다.** 측정에서 병목이 나오지 않으면 도입하지 않는다. 이미 있는
구현을 "써야 해서" 쓰는 것은 §16의 반대다. `Arena`는 소멸자를 호출하지 않으므로
POD에만 쓰고, 소멸자가 필요한 타입은 `Pool`을 쓴다.

### 5.8 World와 Scene

- Phase 1은 `renderer/world`만 쓴다. 지속 상태(오브젝트, GPU 핸들)를 소유한다.
  draw 목록은 프레임마다 다시 만들어지므로 `FrameContext`가 든다.
- `FrameContext`는 매 프레임 새로 만든다. 카메라, 컬링 결과(`DrawList`), 이번
  프레임의 임시 배열이 여기에 산다. 지속 상태와 임시 상태를 섞지 않는다.
- `engine/scene`은 Phase 2의 `SceneProxy` 경계가 확정될 때 채운다. 지금 채우면
  책임이 겹친 두 구조를 동시에 유지하게 된다.

---

## 6. 숫자를 읽는 법

### 6.1 지표 정의

| 지표 | 정의 | 읽는 법 |
|---|---|---|
| `cpu frame` | 프레임 시작 → submit 반환 | 프레임 예산(16.67ms @60)과 비교 |
| `gpu frame` | timestamp start → end | CPU와 독립적으로 본다 |
| `fence wait` | 레인 fence를 기다린 시간 | 크면 GPU가 밀리고 있다 |
| `draw calls` | 제출한 draw 수 | 컬링 전/후를 함께 본다 |
| `throughput` | objects/sec (월드 오브젝트 기준) | 설정 간 비교용 |
| `worker util` | busy / (workers × frame) | 병렬화 효율. 낮으면 분배 불균형 |

### 6.2 병목 판정 순서

```text
cpu frame과 gpu frame을 비교한다
├─ 둘 다 예산을 넘고 비슷하다 → 혼합
│    → 비중을 기록하고 둘 다 후보로 둔다
├─ cpu만 예산을 넘는다 → CPU-bound
│    → 1-B 진행
├─ gpu만 예산을 넘는다 → GPU-bound
│    → 1-B는 인프라만, 배칭은 Phase 3 후보
└─ 둘 다 예산 이하인데 fps가 낮다 → present/vsync 또는 측정 문제
     → present mode와 계측 지점 확인
```

**VSync 주의.** FIFO에서는 프레임 간격이 디스플레이 주사율에 묶인다(60Hz면
16.67ms). `cpu frame`이 그보다 작아도 fps가 60으로 보일 수 있으므로, **fps가 아니라
`cpu frame`과 `gpu frame`으로 판정**한다. 필요하면 벤치마크에서만 present mode를
바꿔 간격의 상한을 없앤다. (게임 기본값은 FIFO 그대로 둔다 — [`frame.md`](../design/frame.md) §5.)

**웜업 주의.** 첫 프레임은 파이프라인·버퍼 생성과 드라이버 준비로 느리다.
최소 60프레임은 측정에서 제외한다. 측정 구간의 앞뒤 경계도 리포트에 남긴다.

---

## 7. 파일 계획

현재 저장소의 `jobs` · `event` · `concurrency` · `profiler`는 **0바이트 스텁**이다.
"이미 있다"와 "동작한다"는 다르다. 아래에서 `구현`은 0바이트 스텁을 채우는 것,
`신규`는 파일 자체를 새로 만드는 것이다.

```text
src/nxt/core/
  concurrency/
    spsc_queue.hpp          구현 — worker별 작업 큐
    mpsc_queue.hpp          구현 — 이벤트 큐 (첫 구현은 mutex)
    work_stealing_deque.hpp 삭제 — 측정 전 도입 금지
  jobs/
    job.hpp                 구현
    job_counter.hpp         신규 (기존 job_counter.cpp는 헤더 없이 0바이트)
    job_counter.cpp         구현
    job_handle.hpp          삭제 — JobHandle을 만들지 않는다 (§8)
    thread_source.hpp       신규
    std_thread_source.hpp/.cpp 신규
    worker.hpp/.cpp         구현
    worker_pool.hpp/.cpp    구현
    scheduler.hpp/.cpp      구현
    parallel.hpp            구현 — parallel_for만
  event/
    event.hpp               구현
    event_queue.hpp/.cpp    구현
    dispatcher.hpp/.cpp     구현
  diagnostics/
    profiler.hpp/.cpp       구현

src/nxt/renderer/
  world/world.hpp           구현 (현재 주석만 있는 스텁)
  world/world.cpp           신규
  draw_list.hpp             신규 — 프레임 draw 목록
  frame_context.hpp/.cpp    확장 — 카메라 · 컬링 결과
  renderer.hpp/.cpp         확장 — draw 목록 소비
  passes/triangle_pass.*    확장 — push constant 사용

shaders/
  triangle.vert/.frag       확장 — 변환 행렬 · 색 입력

src/nxt/graphics/
  rhi/rhi.hpp               확장 — query.hpp 포함
  rhi/resource/pipeline.hpp 확장 — push constant 범위
  rhi/frame/command_buffer.hpp 확장 — pushConstants 기록
  rhi/frame/query.hpp       신규 — timestamp query 최소 API
  backends/vulkan/...       구현

src/nxt/engine/
  frame_stats.hpp/.cpp      신규 — 프레임 지표 집계
  frame_loop.hpp/.cpp       확장 — FrameStats 훅
  engine.hpp/.cpp           확장 — 벤치마크 모드 · 종료 순서

apps/sandbox/main.cpp       CLI · 리포트
```

이 Phase에서 RHI에 더하는 것은 **두 가지뿐**이다. 오브젝트 변환을 위한
push constant와, GPU 시간을 재기 위한 timestamp query다. 그 이상은 측정 결과가
요구할 때 추가한다.

---

## 8. 하지 않는 것

| 항목 | 이유 | 도입 시점 |
|---|---|---|
| work stealing | 불균형이 측정되지 않았다 | 1-D에서 util이 낮으면 Phase 2 후보 |
| lock-free MPSC | 계약은 지금, 구현은 경합 측정 후 | 1-C5/1-D 결과 |
| `parallel_invoke` | 독립 작업이 아직 없다 (§10) | Phase 2 |
| JobHandle · 취소 | 개별 잡을 추적할 요구가 없다. 카운터로 충분하다 | 요구가 생길 때 |
| 렌더 스레드 | [`scene_world.md`](../design/scene_world.md) §6의 기준 미충족 | 측정으로 필요해질 때 |
| render graph · multi-pass · shadow | Phase 3 | Phase 3 |
| asset 로딩 | Phase 2 | [Phase 2](phase2.md) |
| scene graph · component | `SceneProxy` 경계 확정 후 | Phase 2 ADR |
| instancing · batching | 이 Phase의 CPU 경로를 가린다 | Phase 3 (기준선과 비교) |
| SIMD · GPU driven | 측정된 병목이 아니다 | 측정 후 |
| refcounting | 공유 요구가 아직 없다 | Phase 2 이후 |

---

## 9. 미해결 — 결정 시점을 정해 둔다

| 질문 | 결정 시점 | 지금의 가정 |
|---|---|---|
| `wait_idle()`이 GPU 한계에서 오래 멈추면? | 1-D | 기다리는 쪽이 옳다. 필요하면 present_wait 검토 |
| Job 할당에 `Pool`이 필요한가? | 1-B5 / 1-D | 측정 전 도입하지 않는다 |
| Worker 수가 코어 수를 넘어야 하나? | 1-D 스윕 | `hardware_concurrency()` |
| timestamp를 지원하지 않는 GPU는? | 1-A2 | fence wait 근사치 + 리포트 표시 |
| Event payload 64B를 넘으면? | 1-C1 | 핸들로 전달한다 |
| MPSC를 lock-free로 바꿀 때? | 1-D | 경합이 측정되면 Phase 2 후보 |
| 디스패치를 전용 스레드로 옮길 때? | 1-C 게이트 | 메인 스레드 프레임 경계 |
| 렌더 스레드를 넣을 때? | [`scene_world.md`](../design/scene_world.md) §6 | 넣지 않는다 |

---

## 10. Phase 2로 넘기는 것

Phase 1은 빠른 엔진을 만들지 않는다. **Phase 2가 비교할 자**를 만든다.

```text
Phase 1 산출물
  ├─ 기준선 표        : objects · draw calls · cpu/gpu frame · 편차
  ├─ 판정 기록        : CPU-bound였는가, 병렬화가 얼마를 벌었는가
  ├─ 런타임 계약      : submit은 메인만, wait_idle의 범위, Event 수명
  └─ 계측 도구        : FrameStats · Profiler · 벤치마크 CLI
```

Phase 2는 이 위에서 다음을 한다.

- glTF 파싱을 Job으로 던지고, 완료를 Event로 받는다.
- 파싱된 CPU 데이터를 `renderer/world`가 GPU로 올린다.
- 업로드 비용을 Phase 1의 기준선과 같은 형식으로 잰다.

**한 문장으로**: 이 Phase는 "10만 개를 그렸다"가 아니라 **"10만 개에서 무엇이
느린지 말할 수 있고, 고친 뒤 얼마나 나아졌는지 같은 자로 잴 수 있다"**에서 끝난다.
