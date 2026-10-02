# Phase 0 — Core Runtime

> 목표: **GPU와 플랫폼 계층 없이 동작하는 CPU 런타임의 최소 실행 경로를 완성한다.**
>
> `Event → MPSC Queue → Scheduler → Worker → Dispatcher → wait_idle()`
>
> 최종적으로 CLI에서 여러 worker가 이벤트를 처리하고 처리량과 지연 시간을 측정할 수 있어야 한다.

---

## 1. 범위

### 구현

```text
src/nxt/core/

handle/
    handle.hpp

time/
    timer.hpp

diagnostics/
    assert.hpp
    log.hpp
    log.cpp
    profiler.hpp
    profiler.cpp

memory/
    allocator.hpp
    linear_arena.hpp
    linear_arena.cpp
    pool.hpp
    frame_allocator.hpp
    frame_allocator.cpp

containers/
    intrusive_list.hpp

concurrency/
    spsc_queue.hpp
    mpsc_queue.hpp
    work_stealing_deque.hpp

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

event/
    event.hpp
    event_queue.hpp
    event_queue.cpp
    dispatcher.hpp
    dispatcher.cpp
```

`nxt_math`는 Phase 0에서 CMake target과 gateway header만 만든다. 실제 수학 타입은 첫 사용 시 구현한다.

### 제외

- Vulkan / RHI / Renderer
- Scene / Asset / ECS
- Platform backend
- GPU resource
- 실제 math 구현
- SIMD 최적화
- CI/CD

---

## 2. 핵심 규칙

### 의존성

`nxt_core`는 C++ 표준 라이브러리만 직접 의존한다.

```text
nxt_core → C++ Standard Library
```

Core에서 다음을 직접 include하지 않는다.

```text
glm
spdlog
fmt
EASTL
stb
windows.h
pthread.h
unistd.h
```

`nxt_math`는 별도 foundation 모듈로 유지하며 Phase 0에서는 Core가 참조하지 않는다.

### 플랫폼 독립성

Core는 OS API를 직접 호출하지 않는다.

Thread backend 등 플랫폼 기능은 Core가 interface를 정의하고 `platform/`이 구현한다.

### Header

모든 public header는 self-contained여야 한다.

```cpp
#include <nxt/core/...>
```

만으로 필요한 선언을 얻을 수 있어야 한다.

`#pragma once`를 사용한다.

### 예외

Core는 exception 없이 빌드 가능해야 한다.

```text
try / catch / throw 금지
-fno-exceptions 지원
```

복구 가능한 실패는 `Error`와 반환값으로 처리하고, 프로그래머 오류는 `NXT_ASSERT`로 검증한다.

### 메모리

Hot path에서 암묵적인 `new/delete`를 사용하지 않는다.

메모리는 `Allocator`를 통해 주입한다.

예외적으로 arena / pool 등의 내부 storage에서 필요한 할당은 해당 자료구조가 소유한다.

mutable global/static state는 사용하지 않는다.

### 정수 / namespace

Public API에서는 고정 폭 정수를 사용한다.

```cpp
std::uint32_t
std::uint64_t
std::int32_t
std::int64_t
```

기본 namespace:

```cpp
nxt::
```

하위 namespace는 디렉터리와 맞춘다.

```cpp
nxt::jobs
nxt::mem
nxt::event
nxt::diag
nxt::concurrency
```

### Thread safety

모든 concurrent public type은 producer / consumer / synchronization contract를 주석으로 명시한다.

| Type | Producer | Consumer | 보장 |
|---|---:|---:|---|
| `SpscQueue` | 1 | 1 | Lock-free |
| `MpscQueue` | N | 1 | Lock-free |
| `WorkStealingDeque` | Owner 1 | Steal N | Lock-free steal |

Atomic ordering은 필요한 최소 수준으로 사용한다.

- 기본: `relaxed`
- synchronization: `acquire/release`
- `seq_cst`는 필요한 경우에만 사용

x86의 메모리 모델을 전제로 하지 않는다.

### Logging

Core는 직접 출력하지 않는다.

```text
std::cout
printf
```

등을 사용하지 않고 `diagnostics/log.hpp`를 경유한다.

Log sink는 외부에서 주입한다.

---

# 3. 구현 순서

## Step 0 — Build / Test

먼저 다음 target을 만든다.

```text
nxt_core
nxt_core_tests
sandbox
```

`nxt_core`는 static library로 빌드한다.

테스트는 doctest를 사용한다.

컴파일 기준:

```text
C++20
MSVC: /W4 /WX
GCC/Clang: -Wall -Wextra -Werror
```

완료 기준:

```text
cmake --build --preset msvc-debug
```

가 성공하고 테스트 실패가 정상적으로 실패 상태를 반환한다.

---

## Step 1 — Handle / Timer / Assert

구현:

```text
handle.hpp
timer.hpp
assert.hpp
```

### Handle

```text
index      : uint32
generation : uint32
```

stale handle을 검출할 수 있어야 한다.

### 완료

- invalid handle
- generation 증가
- stale handle 검출
- timer 측정
- assert macro 동작

---

## Step 2 — Diagnostics

구현:

```text
log.hpp
log.cpp
```

구조:

```text
Logger → Sink
```

여러 worker에서 동시에 로그를 기록해도 메시지가 손실되거나 서로 섞이지 않아야 한다.

### 완료

- single-thread logging
- multi-thread logging
- sink injection
- thread-safe output

---

### 3. Memory

메모리 시스템은 **CPU Runtime용 메모리와 Renderer용 메모리를 분리**한다.

#### Core Runtime

```text
src/nxt/core/memory/
├── allocator.hpp
├── pool.hpp
└── arena.hpp
```

- `allocator.hpp`
  - 런타임의 기본 allocation abstraction
  - 사용자 정의 allocator 주입을 지원할 수 있는 최소 인터페이스
- `pool.hpp`
  - 고정 크기 객체의 반복적인 allocation/deallocation을 위한 pool
  - Job/Event 등 런타임 객체의 allocation 최적화에 사용
- `arena.hpp`
  - 수명이 명확한 임시 메모리를 위한 linear/arena allocation
  - 개별 객체 단위 deallocation보다 전체 reset이 적합한 경우 사용

Phase 0에서는 **필요한 메모리 패턴을 확인하면서 최소 구현만 진행한다.**
메모리 시스템 자체를 완성하는 것을 목표로 하지 않는다.

#### Renderer

GPU 및 렌더링 관련 메모리는 Core에서 분리한다.

```text
src/nxt/renderer/memory/
├── gpu_allocator.hpp
├── buffer_allocator.hpp
└── image_allocator.hpp
```

- `gpu_allocator.hpp`
  - Vulkan/GPU memory allocation 추상화
- `buffer_allocator.hpp`
  - GPU buffer 관련 allocation 관리
- `image_allocator.hpp`
  - GPU image/image resource 관련 allocation 관리

Renderer 메모리 시스템은 **Phase 0 범위에서 구현하지 않는다.**

#### Memory Rules

- Core memory는 OS/GPU API에 직접 의존하지 않는다.
- Renderer memory는 Vulkan 도입 이후 별도로 설계한다.
- 모든 allocation을 커스텀 allocator로 대체하려고 하지 않는다.
- 실제 allocation 패턴과 성능 요구가 확인된 경우에만 `Pool`, `Arena` 등의 최적화를 적용한다.
- 프레임 단위 allocation은 Renderer의 frame lifecycle이 정의된 이후 별도로 설계한다.
- `FrameAllocator`는 Core에 두지 않는다.

---

## Step 4 — Containers / Queues

구현:

```text
intrusive_list.hpp
spsc_queue.hpp
mpsc_queue.hpp
```

MPSC는 Phase 0의 event enqueue 경로에 사용한다.

초기 구현은 correctness와 검증을 우선한다.

### 완료

- 기본 동작
- full / empty 처리
- multi-producer stress test
- memory ordering 검증
- TSan 검증

---

## Step 5 — Jobs

구현:

```text
job.*
job_handle.*
job_counter.*
thread_source.*
std_thread_source.*
worker.*
worker_pool.*
scheduler.*
parallel.hpp
```

기본 실행 경로:

```text
Scheduler
    ↓
WorkerPool
    ↓
Worker
    ↓
Job
```

`ThreadSource`는 Core가 interface를 정의하고 Phase 0에서는 `StdThreadSource`를 사용한다.

### Worker

worker는 다음만 담당한다.

- job 실행
- queue 소비
- idle wait
- shutdown

### Scheduler

다음 API를 제공한다.

- job submit
- `wait_idle()`
- worker 수 설정

### 완료

- job 실행
- completion counter
- worker pool lifecycle
- `wait_idle()`
- `parallel_for`
- `parallel_invoke`

---

## Step 6 — Work Stealing

`WorkStealingDeque`를 worker에 연결한다.

단, **전체 경로를 먼저 단순 queue 기반으로 검증한 후 교체한다.**

```text
Phase 0-A

Global Queue
    ↓
Workers
```

↓

```text
Phase 0-B

Worker Local Queue
    ↓
Work Stealing
```

목표는 최적화보다 correctness다.

### 완료

- owner push/pop
- concurrent steal
- stress test
- TSan 검증

---

## Step 7 — Event

구현:

```text
event.hpp
event_queue.*
dispatcher.*
```

Event는 RTTI를 사용하지 않는다.

```cpp
using EventTypeId = std::uint32_t;
```

구조:

```text
Event
 ├── EventTypeId
 └── Payload
```

### EventQueue

```text
Producer N
    ↓
MPSC Queue
    ↓
Consumer 1
```

### Dispatcher

- handler 등록
- handler 제거
- EventTypeId 기반 dispatch
- handler 호출

### 완료

다음 경로가 정상적으로 동작한다.

```text
Event
  ↓
MPSC Queue
  ↓
Scheduler
  ↓
Worker
  ↓
Dispatcher
  ↓
Handler
  ↓
Job Counter
  ↓
wait_idle()
```

---

## Step 8 — Profiler

기본 profiler만 구현한다.

측정 대상:

```text
event enqueue → consume latency
job wait time
worker busy time
worker idle time
handler execution time
```

Profiler의 출력은 Logger를 사용한다.

### 완료

Profiler ON/OFF 상태의 runtime overhead를 측정한다.

---

## Step 9 — Sandbox Integration

`sandbox`를 CPU runtime demo로 사용한다.

입력:

```text
worker count
event count
producer count
```

출력:

```text
processed events
failed events
total time
throughput
average latency
maximum latency
worker utilization
```

예:

```text
NXT CPU Runtime Demo

workers     : 8
producers   : 4
events      : 1,000,000

processed   : 1,000,000
failed      : 0

time        : ...
throughput  : ... events/sec
avg latency : ...
max latency : ...
```

---

# 4. 검증

## Build

```text
cmake --build --preset msvc-debug
```

무경고 성공.

## Unit Test

모든 Core 모듈의 기본 동작을 검증한다.

## Stress Test

다음 concurrency component를 반복 검증한다.

```text
SPSC
MPSC
WorkStealingDeque
JobCounter
Scheduler
EventQueue
```

## TSan

Concurrency 관련 테스트는 별도 Linux/Clang 또는 GCC TSan configuration에서 실행한다.

## Scalability

worker 수를 변경하여 다음을 측정한다.

```text
1
2
4
8
...
```

측정:

```text
execution time
throughput
latency
worker utilization
```

성능 문제는 추측하지 않고 측정 결과를 기준으로 개선한다.

---

# 5. Phase 0 완료 기준

다음 조건을 모두 만족한다.

- `nxt_core`가 warning-free로 빌드된다.
- Core unit test가 모두 통과한다.
- TSan에서 concurrency path에 data race가 없다.
- Core가 GPU / Window 없이 단독 실행된다.
- Event → Queue → Scheduler → Worker → Dispatcher → Handler → wait_idle 경로가 동작한다.
- worker 수에 따른 처리량과 latency를 측정할 수 있다.
- Core의 public concurrent type에 thread-safety contract가 명시되어 있다.
- Core가 플랫폼 API나 third-party library에 직접 의존하지 않는다.
- hot path의 ownership과 allocation 규칙이 명확하다.

---

# 6. 확정 사항

### D1 — Core dependency

Core의 직접 dependency는 C++ Standard Library뿐이다.

### D2 — Thread backend

Core가 interface를 정의하고 platform이 구현한다.

Phase 0의 기본 구현은 `StdThreadSource`다.

### D3 — Error handling

`Error + return value`를 기본으로 사용한다.

전 계층에 `Result<T>`를 강제하지 않는다.

### D4 — Math

`nxt_math + glm`을 별도 foundation module로 유지한다.

Phase 0에서는 골격만 만든다.

### D5 — Testing

doctest는 test target에서만 사용한다.

### D6 — Formatting

`.clang-format`을 단일 기준으로 사용한다.

---

# 7. 주요 리스크

| Risk | 대응 |
|---|---|
| MPSC memory ordering 오류 | C++ memory model 기준 설계 + TSan + stress test |
| Work stealing 구현 오류 | global queue로 먼저 검증한 후 교체 |
| Job overhead | `parallel_for` benchmark로 측정 |
| Queue contention | profiler로 측정 |
| Allocator 설계 후회 | ownership / lifetime을 먼저 고정 |
| Platform API 침투 | Core에서 OS header 직접 include 금지 |

---

# 8. 핵심 원칙

```text
Correctness
    ↓
Measurement
    ↓
Optimization
```

Phase 0에서는 성능을 추측해서 구조를 복잡하게 만들지 않는다.

먼저 다음 경로를 **단순하고 검증 가능한 형태로 완성한다.**

```text
Event
 → MPSC Queue
 → Scheduler
 → Worker
 → Dispatcher
 → Handler
 → Job Counter
 → wait_idle()
```

그 후 profiler와 benchmark를 통해 실제 병목을 확인하고 최적화한다.
