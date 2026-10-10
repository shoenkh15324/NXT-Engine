# Phase 1 — 10만 삼각형

> 목표: **삼각형 10만 개가 돌아가는 씬을 만들고, 병목 위치를 측정한다.**
>
> CPU 런타임을 **이 Phase에서** 넣는다. 그보다 먼저 넣으면 이유가 없는 최적화가 된다.

---

## 1. 왜 CPU 런타임이 여기서 들어가는가

이전 로드맵은 CPU 런타임을 Phase 0에 배치했다. 삼각형 하나에 job system이 필요 없다는
사실 때문이었다. 삼각형 하나에는 절대 필요하지 않다. 그러니 먼저 만들면
근거 없는 선행 최적화가 된다.

여기서는 순서가 뒤집힌다.

```text
Phase 0 : 삼각형 1개      → 지루하다
Phase 1 : 삼각형 10만 개   → 처음 지루함이 현실이 된다
                            → "왜 안 빨라지지?"는 질문이 자연히 생긴다
                            → 그때 병렬을 넣는다
```

`CODING_RULES.md` §16은 "필요성이 측정되거나 설계 요구가 명확할 때 도입한다"고 한다.
이 Phase의 1-A가 그 측정을 만드는 단계다. **1-A의 결과가 1-B의 근거가 된다.**

## 2. 구현 파일

### 2.1 `nxt_core` — concurrency

```text
src/nxt/core/
    concurrency/
        spsc_queue.hpp
        mpsc_queue.hpp
```

SPSC는 Phase 0에서 만들었고, MPSC가 여기서 추가된다.

### 2.2 `nxt_core` — jobs

```text
    jobs/
        job.hpp
        job_handle.hpp
        job_counter.hpp
        job_counter.cpp
        thread_source.hpp
        std_thread_source.hpp
        std_thread_source.cpp
        worker.hpp
        worker.cpp
        worker_pool.hpp
        worker_pool.cpp
        scheduler.hpp
        scheduler.cpp
        parallel.hpp
```

디스크 상태에 대한 사실관계:

- `thread_source.hpp`, `std_thread_source.{hpp,cpp}`, `job_counter.hpp`는 **아직 없다.**
  이전 로드맵에 적혀 있었으나 파일이 만들어지지 않았다. 이 Phase에서 생성한다.
- `job_counter.cpp`는 **헤더 없이 혼자 존재한다** (0바이트). `job_counter.hpp`와 함께 정리한다.
- 나머지 jobs 스텁은 0바이트 placeholder다.

### 2.3 `nxt_core` — event, profiler

```text
    event/
        event.hpp
        event_queue.hpp
        event_queue.cpp
        dispatcher.hpp
        dispatcher.cpp

    diagnostics/
        profiler.hpp
        profiler.cpp
```

`Event`는 RTTI를 쓰지 않는다.

```cpp
using EventTypeId = std::uint32_t;
```

```text
Event { EventTypeId, Payload }
    ↓
MPSC Queue
    ↓
Scheduler → Worker → Dispatcher → Handler → JobCounter → wait_idle()
```

`Event`와 `Dispatcher`는 렌더링과 무관한 CPU 런타임 기능이다. RHI 경계를 건드리지 않는다.

### 2.4 `nxt_renderer` — world

```text
src/nxt/renderer/
    world/world.hpp
    world/world.cpp
```

`World`는 씬의 도가 있는 상태와 그릴 대상 목록을 소유한다.

> **주의:** 저장소에는 `renderer/world/world.hpp`("씬 컬렉션 관리")와
> `engine/scene/scene.hpp`("씬 그래프 노드와 컴포넌트 관리")가 함께 있고 책임이 겹친다.
> **Phase 2 착수 전에 이 경계를 ADR로 확정해야 한다.** 지금은 `renderer/world`만 쓴다.

### 2.5 삭제 대상

```text
src/nxt/core/concurrency/work_stealing_deque.hpp   (삭제 — 측정 전 도입 금지)
```

work stealing은 단일 큐로 병목이 확인된 **뒤에만** 만든다. 1-C의 측정 결과에 따라
다음 Phase로 미룰지 그때 결정한다.

---

## 3. 마일스톤

### 1-A — 씬과 병목 측정 (병렬 없음)

`nxt_renderer::World`에 삼각형 N개를 두고 **단일 스레드로** 그린다.
**이때 병렬은 넣지 않는다.**

**목적**: 병목이 어디에 있는지 측정한다. 추측으로 병렬 넣지 않기 위해.

- 물체 수는 명령행으로 지정한다 (기본 100,000)
- 물체당 단순 AABB 컬링을 넣는다
- profiler로 CPU 시간을 측한다

**완료 기준**

- [ ] 물체 수를 바꾸면 그만큼 그려진다
- [ ] CPU 프레임 시간과 GPU 프레임 시간이 따로 측정된다
- [ ] 병목이 CPU인지 GPU인지 **수치로 확인된다**

### 1-B — CPU 런타임

1-A에서 CPU가 병목이었다면, 병렬로 대응한다.

```text
Scheduler → WorkerPool → Worker → Job
```

- `thread_source`를 interface로 두고 `std_thread_source`로 구현한다.
  core가 OS 스레드 API를 직접 부르지 않게 하기 위해서다.
- worker는 job 실행, queue 소비, idle 대기, 종료만 담당한다.
- `parallel_for`, `parallel_invoke`를 제공한다.

**worker 수 기본값은 `std::thread::hardware_concurrency()`다.**

`hardware_concurrency() - 1`은 **호출 스레드도 job을 처리할 때만** 맞다.
TBB와 EnkiTS가 코어 수를 그대로 쓰는 이유다. NXT는 worker가 독립적으로
일하고 메인 스레드는 `wait_idle()`에서 블로킹하므로, 코어 수가 정확한 답이다.
worker 수를 인자로 받는 API는 나중에 필요해지면 그때 추가한다.

**완료 기준**

- [ ] 병렬 컬링이 동작하고 1-A보다 빨라진다
- [ ] `wait_idle()`이 실제 동기점을 보장한다
- [ ] worker 수를 바꾸면 처리량이 변한다
- [ ] TSan 통과

### 1-C — Event, Dispatcher, 벤치마크

CPU 런타임 위에서 이벤트를 흘린다. dispatcher가.handler를 부르고
`wait_idle()`로 닫힌다.

**완료 기준**

- [ ] `Event → MPSC → Scheduler → Worker → Dispatcher → Handler` 경로가 동작한다
- [ ] profiler ON/OFF의 런타임 오버헤드를 측정했다
- [ ] sandbox가 아래 지표를 출력한다

```text
NXT CPU Runtime Benchmark

workers        : 8
objects        : 100,000
draw calls     : 100,000

cpu frame      : ... ms
gpu frame      : ... ms
throughput     : ... objects/sec
worker util    : ... %
```

- [ ] `validate layers` 기준선도 함께 기록한다

---

## 4. Memory 모듈의 첫 소비자

`Arena`와 `Pool`은 구현되어 있으나 **지금까지 소비자가 없었다.**
이 Phase에서 처음으로 쓰일 수 있는 곳이 생긴다.

| 대상 | 도구 | 조건 |
|---|---|---|
| Job 객체 | `Pool<Job>` | job 할당이 측정된 병목일 때 |
| 프레임 컬링 데이터 | `Arena` | 컬링 중간 결과가 프레임 단위 수명을 가질 때 |

**둘 다 조건부다.** 1-C의 측정에서 병목이 나오지 않으면 도입하지 않는다.
이미 있는 구현을 쓰거나 삭제하는 것은 측정 후에 결정한다.

`nxt_core`이 제공하므로 `nxt_renderer`는 `nxt_core::memory`만 알면 된다.
렌더러가 arena를 직접 소유하지 않는다.

## 5. 제외

- Work stealing → **측정 후 판단.** lock-free 구조를 측정 없이 넣지 않는다
- Render graph, multi-pass, depth → Phase 3 (로드맵 미작성)
- Asset 로딩 → [Phase 2](phase2.md)
- Scene graph / component → `engine/scene` 경계 확정 후. [Phase 2](phase2.md)
- GPU skinning, compute → Phase 4 이후

## 6. 미해결

| 질문 | 비고 |
|---|---|
| `wait_idle()`이 멈추지 않으면 | GPU 한계에서 어떻게 할지. 1-C에서 실측으로 결정 |
| job 할당에 `Pool`이 필요한가 | 1-C 측정 결과로 결정 |
| worker 수가 CPU 코어 수를 넘어야 하는가 | 1-C 측정 결과로 결정 |
| 렌더 스레드를 도입할 것인가 | [`../design/scene_world.md`](../design/scene_world.md) §6의 기준 사용 |

## 7. Phase 1 완료 시점

```text
nxt_engine  →  nxt_renderer (World)  →  nxt_graphics
     │                │
     └── CPU 런타임 ──┘   ← Phase 0에서 없던 것이 추가됨
```

단위 프레임 비용이 숫자로 말할 수 있고, 병목 위치가 측정으로 확인된 상태.