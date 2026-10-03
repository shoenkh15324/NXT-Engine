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
    handle_manager.hpp

time/
    timer.hpp

diagnostics/
    assert.hpp
    assert.cpp
    log.hpp
    log.cpp
    profiler.hpp
    profiler.cpp

memory/
    allocator.hpp
    pool.hpp
    arena.hpp

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

# 2. 구현 순서

## Step 1 — Handle / Timer / Assert

구현:

```text
handle.hpp
handle_manager.hpp
timer.hpp
assert.hpp
assert.cpp
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

assert macro의 검증 범위는 조건이 참인 경로와 조건 평가 여부에 한정된다.
실패 경로는 프로세스를 끝내므로 in-process 테스트로 확인할 수 없다. 실패
보고가 올바른 정보를 전달하는지는 `report`와 분리된 `notify` 경로로 검증한다.

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