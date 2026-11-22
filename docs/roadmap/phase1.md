# Phase 1 — 10만 삼각형

> **목표**: 10만 오브젝트 씬에서 **무엇이 느린지 숫자로 답하고**, 그 위에서 CPU 런타임과
> 이벤트 경로를 검증한다.
>
> 최종 산출물은 빠른 렌더러가 아니라 **병목 판단 절차와 기준선**이다.

**상태**: 1-A2까지 완료. 1-A3부터 진행한다.

---

## 1. 마일스톤과 게이트

| 마일스톤 | 답하려는 질문 | 산출물 |
|---|---|---|
| **1-A 기준선** | 병목이 CPU인가 GPU인가? | 10만 오브젝트 + 계측 + 기준선 표 |
| **1-B CPU 런타임** | CPU 병목을 병렬로 줄일 수 있나? | Job · Worker · Scheduler · parallel_for |
| **1-C 이벤트** | 런타임 위에서 이벤트를 안전하게 흘릴 수 있나? | Event · MPSC 큐 · Dispatcher |
| **1-D 벤치마크** | 이 숫자가 재현되는가? | CLI + 비교 표 + 기준선 기록 |

```text
1-A ──▶ [게이트 A] ──▶ 1-B ──▶ [게이트 B] ──▶ 1-C ──▶ [게이트 C] ──▶ 1-D
측정      CPU 병목?     병렬화     이득 있음?     이벤트     재현 가능?
```

게이트를 통과하지 못하면 다음 마일스톤으로 넘어가지 않는다. 각 게이트의 판정은
문서로 남긴다.

---

## 2. 공통 원칙

| 원칙 | 의미 |
|---|---|
| **측정 → 최적화** | 1-A 없이 1-B를 시작하지 않는다 |
| **계약 먼저, 구현은 단순하게** | 생산자·소비자·동기화·수명을 먼저 정하고, 첫 구현은 가장 단순하게 만든다 |
| **소유권 명시** | Job · Event · World의 owner가 코드에 보인다 |
| **요구 없는 추상화 금지** | lock-free · work stealing · parallel_invoke는 측정 후에만 |
| **경계 유지** | `core`는 `graphics`를 모른다. 이벤트는 렌더링을 모른다 |

---

## 3. 지표와 판정

| 지표 | 정의 |
|---|---|
| `cpu frame` | 프레임 시작 → submit 반환 |
| `gpu frame` | timestamp start → end |
| `fence wait` | 레인 fence를 기다린 시간 |
| `cull` / `record` / `submit` | Profiler가 재는 CPU 구간 |
| `draw calls` | 제출한 draw 수 (visible / total) |
| `throughput` | objects/sec |
| `worker util` | busy / (workers × frame) |

- 판정은 fps가 아니라 `cpu frame`과 `gpu frame`으로 한다. FIFO에서는 프레임 간격이
  주사율에 묶인다.
- 워밍업 60프레임은 측정에서 제외한다.
- Debug 숫자는 성능이 아니다. 판정은 Release 또는 RelWithDebInfo에서만 한다.

---

## 4. 1-A — 기준선

**목표**: 최적화 없이 10만 오브젝트의 비용을 측정한다.

**제약**

- 단위 삼각형 하나와 오브젝트별 변환·색만 쓴다. push constant로 전달한다.
- 오브젝트당 draw call 하나를 유지한다. 이 Phase가 재는 대상은 CPU 기록·제출 경로다.
- 배치는 고정 시드 그리드로 하고, 일부는 프러스텀 밖에 둔다.
- 카메라는 고정한다. 컬링 외의 변수를 늘리지 않는다.
- `renderer/world`만 쓴다. `engine/scene`은 건드리지 않는다.

**순서**: 1-A3 → 1-A4 → 1-A5 → 1-A6

### 1-A1 — World와 push constant (완료)

- **목표:** N개 오브젝트를 오브젝트당 1 draw로 그린다.
- **작업 내용:**
  - `renderer/world`에 오브젝트 목록(변환·AABB·색)을 둔다.
  - 셰이더에 변환·색을 push constant로 넘긴다.
  - `sandbox --objects N`으로 오브젝트 수를 바꾼다.
- **완료 조건:** `--objects N`의 draw call 수가 N과 일치한다.

### 1-A2 — FrameStats · Profiler · GPU timestamp (완료)

- **목표:** CPU 프레임과 GPU 프레임을 분리해서 잰다.
- **작업 내용:**
  - `FrameStats`가 cpu frame · gpu frame · fence wait · draw 수를 링 버퍼에 모은다.
  - `Profiler` RAII scope가 `record` · `submit` 구간을 잰다.
  - 프레임마다 timestamp 두 개를 찍고, fence 신호 뒤에 읽는다.
  - 1초에 한 번만 요약을 로그로 남긴다.
- **완료 조건:** 로그에 `cpu` · `gpu` · `fence` · `draws`가 따로 나온다.

### 1-A3 — 프러스텀 AABB 판정

- **목표:** 오브젝트 AABB가 고정 프러스텀과 겹치는지 판정한다.
- **작업 내용:**
  - `math/frustum.hpp`에 클립 공간 기준 고정 프러스텀(6평면)을 둔다.
  - `math/aabb.hpp`에 AABB-프러스텀 교차 판정을 추가한다.
  - 내부·외부·경계 세 경우의 단위 테스트를 둔다.
- **완료 조건:** 세 경우의 단위 테스트가 통과한다.

### 1-A4 — DrawList와 단일 스레드 컬링

- **목표:** 컬링 결과만 renderer가 그리게 한다.
- **작업 내용:**
  - 1-A3의 판정을 사용해 `renderer/draw_list.hpp`에 visible 목록을 만든다.
  - `FrameContext`가 컬링 결과를 들고, `Renderer::record`가 DrawList를 소비한다.
  - 컬링은 메인 스레드에서 한 번 돈다.
  - `sandbox --culling on|off`로 전환한다.
  - 전체 오브젝트 수와 visible draw 수를 함께 로그로 남긴다.
- **완료 조건:** `--culling off`면 `draws=N`, `on`이면 `draws=M`이고 두 수가 함께 찍힌다. Draw 순서는 월드 순서와 같다.

### 1-A5 — 워밍업 제외와 중앙값 · p95

- **목표:** 한 번의 측정값이 아니라 분포로 프레임 비용을 요약한다.
- **작업 내용:**
  - `FrameStats`가 앞 60프레임을 워밍업으로 제외한다.
  - 평균 대신 중앙값과 p95를 계산한다.
  - 1초 요약과 종료 요약을 중앙값 · p95로 바꾼다.
- **완료 조건:** 같은 설정 3회 실행의 중앙값 · p95 편차가 기록된다.

### 1-A6 — 기준선 측정과 게이트 A 판정

- **목표:** 이후 비교의 기준이 되는 기준선 표를 만든다.
- **작업 내용:**
  - `objects=100000`으로 컬링 off/on을 각 3회 실행한다.
  - cpu frame · gpu frame · fence wait · draw 수 · throughput을 기록한다.
  - `docs/benchmarks/phase1-baseline.md`(신규)에 표를 남긴다.
  - 게이트 A를 판정한다.
- **완료 조건:** 기준선 표와 게이트 A 판정이 문서에 남는다.

**게이트 A — 1-B의 범위를 정한다**

- `cpu frame`이 예산을 넘고 `gpu frame`이 그보다 작다 → CPU-bound. 1-B 전체를 진행한다.
- `gpu frame`이 `cpu frame` 이상이거나 `fence wait`가 크다 → GPU-bound. 1-B는 인프라
  (1-B1~1-B7, 1-B9)만 하고 1-B8은 생략한다. 배칭·인스턴싱은 Phase 3 후보로 기록한다.
- 둘 다 예산 이하인데 fps만 낮다 → present/vsync와 계측 지점을 먼저 확인한다.
- 실행마다 편차가 크다 → 1-A5로 돌아간다. 이 숫자는 기준선이 아니다.

---

## 5. 1-B — CPU 런타임

**목표**: CPU 병목을 병렬로 줄일 실행 기반을 만들고, 컬링에 적용해 이득을 측정한다.
Phase 2의 비동기 로딩도 이 런타임을 전제로 한다.

**제약**

- `thread_source` 인터페이스와 `std_thread_source` 구현을 둔다. 테스트는 fake source를 쓴다.
- worker 수는 `std::thread::hardware_concurrency()`다. 0이면 1로 보정한다.
- 작업 큐는 worker별 SPSC다. **잡 제출은 메인 스레드만** 한다.
- `JobCounter`는 호출자가 소유한다. `wait()`가 끝나기 전에 파괴하지 않는다.
- `wait_idle()`은 호출 이전에 제출된 모든 잡이 끝나야 반환한다.
- 병렬 컬링은 청크별 DrawList를 만든 뒤 **청크 순서대로** 합친다.
- work stealing과 `parallel_invoke`는 만들지 않는다.

**순서**: 1-B1 → 1-B2 → 1-B3 → 1-B4 → 1-B5 → 1-B6 → 1-B7 → 1-B8 → 1-B9

### 1-B1 — thread_source

- **목표:** 스레드 생성·종료를 테스트 가능한 인터페이스로 분리한다.
- **작업 내용:**
  - `core/jobs/thread_source.hpp`에 인터페이스를 둔다.
  - `std_thread_source.hpp/.cpp`에 `std::thread` 구현을 둔다.
  - 테스트용 fake source를 둔다.
- **완료 조건:** fake source로 생성·종료 테스트가 통과하고, std source로 실제 스레드를 만들고 join한다.

### 1-B2 — Job과 JobCounter

- **목표:** 잡 하나의 실행 단위와 N개 완료 대기를 제공한다.
- **작업 내용:**
  - `jobs/job.hpp`에 실행 함수와 카운터 포인터를 둔다.
  - `jobs/job_counter.hpp/.cpp`에 atomic 카운터와 `wait()`를 둔다.
  - 소유권 계약을 주석과 assert로 고정한다. `job_handle.hpp` 스텁은 삭제한다.
- **완료 조건:** N개 완료 후 `wait()`가 반환하고, 대기 전 파괴를 assert가 막는다.

### 1-B3 — worker별 SPSC 큐

- **목표:** 단일 생산자·단일 소비자 작업 큐를 만든다.
- **작업 내용:**
  - `concurrency/spsc_queue.hpp`에 push/pop을 둔다.
  - 생산자는 메인 스레드, 소비자는 worker 하나로 제한한다.
  - `work_stealing_deque.hpp` 스텁은 삭제한다.
- **완료 조건:** 1 생산자/1 소비자 스트레스에서 유실과 순서 역전이 0이다.

### 1-B4 — Worker

- **목표:** Worker가 자기 큐의 잡을 실행하고 정지 신호에서 종료한다.
- **작업 내용:**
  - `jobs/worker.hpp/.cpp`에 실행 루프를 둔다.
  - 큐에서 꺼내 실행한 뒤 반드시 `JobCounter`를 감소시킨다.
  - 정지와 join 경로를 둔다.
- **완료 조건:** 제출한 잡 N개가 모두 실행되고, 정지 후 스레드가 join된다.

### 1-B5 — WorkerPool

- **목표:** N개 worker의 수명을 관리한다.
- **작업 내용:**
  - `jobs/worker_pool.hpp/.cpp`에 생성·시작·정지·join을 둔다.
  - worker 수는 `hardware_concurrency()`로 정하고 0이면 1로 보정한다.
- **완료 조건:** N개 worker가 생성·정지·join되고, 0코어 보정 테스트가 통과한다.

### 1-B6 — Scheduler와 wait_idle

- **목표:** 제출 정책과 프레임 동기점을 제공한다.
- **작업 내용:**
  - `jobs/scheduler.hpp/.cpp`에 라운드 로빈 제출을 둔다.
  - `wait_idle()`은 호출 전에 제출된 잡만 기다린다.
  - 메인 스레드 외 제출은 assert로 막는다.
- **완료 조건:** submit → `wait_idle()` 후 모든 잡이 끝나고, 이후 제출은 대기 대상이 아니다.

### 1-B7 — parallel_for

- **목표:** 범위 작업을 청크로 나눠 병렬 실행한다.
- **작업 내용:**
  - `jobs/parallel.hpp`에 `parallel_for`를 둔다.
  - 청크 수는 `workers × 4`를 넘지 않게 한다.
  - 완료 대기는 `JobCounter` 하나로 한다.
- **완료 조건:** 결과가 순차 실행과 같고, 청크 수 상한이 지켜진다.

### 1-B8 — 병렬 컬링

- **목표:** 1-A4의 컬링을 `parallel_for`로 바꾼다.
- **작업 내용:**
  - 청크별로 DrawList를 만들고 청크 순서대로 합친다.
  - `Renderer`가 `Scheduler`를 통해 컬링을 병렬화한다.
- **완료 조건:** draw 순서와 수가 1-A4와 같고 `cull` 시간이 줄어든다. 게이트 A가 CPU-bound일 때만 수행한다.

### 1-B9 — 동시성 검증

- **목표:** 동시성 계약을 반복 실행으로 검증한다.
- **작업 내용:**
  - 스트레스 100회, fake source 결정적 실행, 불변식 assert를 둔다.
  - 가능한 환경에서 TSan을 돌린다. MSVC에는 TSan이 없으므로 WSL/Linux 경로를 먼저 정한다.
- **완료 조건:** 스트레스 실패 0. TSan 경로가 없으면 대체 검증과 한계를 기록한다.

**게이트 B — 이벤트를 얹을 것인가**

- 병렬 컬링으로 `cpu frame`이 줄었다 → 1-C로 진행한다.
- 1-B8을 생략한 경우 → 1-B9의 인프라 검증이 통과하면 1-C로 진행한다.
- 줄지 않았거나 느려졌다 → 원인을 기록한다. 1-C는 진행하되 런타임 추가 최적화는 하지 않는다.
- 스트레스 또는 TSan 실패 → 1-C 금지. 1-B를 먼저 고친다.
- worker util이 낮다 → work stealing은 Phase 2 후보로 기록한다.

---

## 6. 1-C — 이벤트

**목표**: 다중 생산자 → 단일 소비자 통지 경로를 렌더링과 분리해 검증한다.
Phase 2의 비동기 로딩 완료 통지가 이 경로를 쓴다.

**제약**

- `EventTypeId`는 `std::uint32_t`다. RTTI와 전역 타입 레지스트리는 쓰지 않는다.
- payload는 64B 이하 POD다. 큰 데이터는 핸들로 보낸다. `Event`는 trivially copyable이다.
- 큐 계약은 MPSC다. 첫 구현은 mutex로 둔다. lock-free는 측정 후에만 검토한다.
- 소비자는 메인 스레드 하나다. `wait_idle()` 직후 프레임 경계에서 비운다.
- 같은 생산자가 넣은 이벤트의 순서만 보장한다.

**순서**: 1-C1 → 1-C2 → 1-C3 → 1-C4 → 1-C5

### 1-C1 — Event와 EventTypeId

- **목표:** 이벤트 값 타입과 타입 ID 계약을 만든다.
- **작업 내용:**
  - `event/event.hpp`에 `EventTypeId`와 고정 크기 payload를 둔다.
  - 큰 데이터는 핸들로 전달하는 규칙을 둔다.
  - trivially copyable과 크기 제한을 `static_assert`로 고정한다.
- **완료 조건:** 크기·trivially copyable 검사가 통과하고, 타입 ID 중복이 0이다.

### 1-C2 — MPSC 큐

- **목표:** 다중 생산자·단일 소비자 이벤트 큐를 만든다.
- **작업 내용:**
  - `concurrency/mpsc_queue.hpp`에 push와 drain을 둔다.
  - 첫 구현은 mutex와 deque로 둔다.
- **완료 조건:** N 생산자/1 소비자 스트레스에서 유실·중복이 0이고, 같은 생산자의 순서가 유지된다.

### 1-C3 — Dispatcher

- **목표:** 타입 ID별 핸들러를 등록하고 호출한다.
- **작업 내용:**
  - `event/dispatcher.hpp/.cpp`에 등록과 디스패치를 둔다.
  - 미등록 타입은 명시적으로 거부한다.
- **완료 조건:** 등록된 타입은 이벤트당 한 번 호출되고, 미등록 타입은 조용히 사라지지 않는다.

### 1-C4 — 프레임 경계 통합

- **목표:** 이벤트를 프레임 경계에서 소비한다.
- **작업 내용:**
  - `event/event_queue.hpp/.cpp`에 1-C2의 `MpscQueue<Event>`를 감싼 이벤트 전용 큐를 둔다.
  - 1-B6의 `wait_idle()` 뒤 메인 스레드에서 큐를 비우고 Dispatcher로 넘긴다.
  - 프레임당 발행 수와 처리 수를 집계한다.
- **완료 조건:** 프레임당 발행 수와 처리 수가 같고, 유실·중복이 0이다.

### 1-C5 — Profiler 오버헤드 측정

- **목표:** 계측 ON/OFF 비용을 측정한다.
- **작업 내용:**
  - `Profiler`의 런타임 토글 경로를 연결한다.
  - 같은 워크로드에서 ON/OFF를 비교한다.
  - 1-D 리포트에 넣을 필드를 확정한다.
- **완료 조건:** ON/OFF 오버헤드 수치가 기록된다. 기본값 정책은 게이트 C에서 정한다.

**게이트 C — Phase 2로 넘어갈 조건**

- 유실·중복 0, 스트레스와 TSan 통과 → 1-D로 진행한다.
- 디스패치가 프레임 예산을 잡아먹는다 → 핸들러를 가볍게 하거나 전용 소비자를 검토한다.
- 큐 경합이 병목이다 → lock-free 교체는 Phase 2 후보로 기록한다. 이 Phase에서는 하지 않는다.
- profiler 오버헤드가 크다 → 기본 OFF, 벤치마크에서만 ON으로 정한다.

---

## 7. 1-D — 벤치마크

**목표**: 같은 입력에서 같은 결론이 나오게 만든다. Phase 2가 이 표를 기준선으로 쓴다.

**제약**

- 빌드는 Release 또는 RelWithDebInfo를 쓴다. Debug는 검증용이다.
- present는 기본 FIFO다. 벤치마크에서만 상한을 해제할 수 있다.
- 입력 배치는 고정 시드다.
- 환경(CPU/GPU/빌드)과 설정을 결과와 함께 기록한다.
- 통계는 1-A5의 중앙값 · p95를 쓰고, 리포트 항목은 3장의 지표 표를 따른다.

**순서**: 1-D1 → 1-D2 → 1-D3 → 1-D4

### 1-D1 — benchmark CLI

- **목표:** 벤치마크 설정을 명령행으로 재현한다.
- **작업 내용:**
  - `sandbox --benchmark`에 `objects` · `frames` · `warmup` · `workers` · `culling` · `profiler` 옵션을 둔다.
  - 잘못된 값을 거부한다.
  - 적용된 설정을 리포트 헤더에 그대로 찍는다.
- **완료 조건:** 옵션 조합이 헤더에 그대로 나오고, 잘못된 값은 실행을 거부한다.

### 1-D2 — 벤치마크 실행 모드

- **목표:** 정해진 프레임만 실행하고 리포트를 남긴다.
- **작업 내용:**
  - `frames`와 `warmup`을 적용한다.
  - 측정이 끝나면 자동으로 종료한다.
  - 종료 시 중앙값 · p95 리포트를 한 번 출력한다.
- **완료 조건:** `--benchmark` 실행이 스스로 종료하고 리포트를 하나 남긴다.

### 1-D3 — 설정 비교

- **목표:** 설정 간 차이를 같은 형식으로 비교한다.
- **작업 내용:**
  - worker 수를 1..N으로 스윕한다.
  - 컬링 on/off, profiler on/off를 비교한다.
  - 각 설정을 3회 실행하고 편차를 기록한다.
- **완료 조건:** 설정별 중앙값 · p95 · 편차가 한 표에 나온다.

### 1-D4 — 기준선 기록과 Phase 2 인계

- **목표:** Phase 1의 결론을 문서로 고정한다.
- **작업 내용:**
  - `docs/benchmarks/phase1-baseline.md`에 환경 · 설정 · 결과 표를 갱신한다.
  - 게이트 A · B · C의 판정을 기록한다.
  - Phase 2가 비교할 항목과 인계 사항을 적는다.
- **완료 조건:** Phase 2가 비교할 기준선 표와 판정 기록이 문서에 있다.

**Phase 1 완료 정의**

- [ ] 10만 오브젝트가 월드에 있고, 컬링을 끄면 draw call 수가 입력과 일치한다.
- [ ] CPU 프레임과 GPU 프레임이 분리 측정되고 병목이 숫자로 설명된다.
- [ ] 병렬 컬링의 전/후가 같은 조건에서 비교된다.
- [ ] `Event → Queue → Dispatcher → Handler`가 유실·중복 없이 닫힌다.
- [ ] 가능한 환경의 TSan과 반복 스트레스가 통과한다.
- [ ] GPU 검증 레이어 VUID/ERROR가 0건이다. (Debug, 검증 레이어 ON)
- [ ] profiler ON/OFF 오버헤드가 기록된다.
- [ ] Phase 2가 쓸 기준선 표가 문서에 있다.

---

## 8. 하지 않는 것

| 항목 | 이유 | 도입 시점 |
|---|---|---|
| work stealing | 불균형이 측정되지 않았다 | 1-D3에서 util이 낮으면 Phase 2 후보 |
| lock-free MPSC | 계약은 지금, 구현은 경합 측정 후 | 1-C5 · 1-D3 결과 |
| `parallel_invoke` | 독립 작업이 아직 없다 | Phase 2 |
| JobHandle · 취소 | 개별 잡 추적 요구가 없다. 카운터로 충분하다 | 요구가 생길 때 |
| 렌더 스레드 | `scene_world.md` §6의 기준 미충족 | 측정으로 필요해질 때 |
| render graph · multi-pass · shadow | Phase 3 | Phase 3 |
| asset 로딩 | Phase 2 | [Phase 2](phase2.md) |
| scene graph · component | `SceneProxy` 경계 확정 후 | Phase 2 ADR |
| instancing · batching | 이 Phase의 CPU 경로를 가린다 | Phase 3 |
| SIMD · GPU driven | 측정된 병목이 아니다 | 측정 후 |
| refcounting | 공유 요구가 아직 없다 | Phase 2 이후 |

---

## 9. 미해결 — 결정 시점

| 질문 | 결정 시점 | 지금의 가정 |
|---|---|---|
| timestamp를 지원하지 않는 GPU는? | 1-A2 (완료) | fence wait 근사치 + 리포트 표시 |
| Job 할당에 `Pool`이 필요한가? | 1-B9 · 1-D3 | 측정 전 도입하지 않는다 |
| Worker 수가 코어 수를 넘어야 하나? | 1-D3 스윕 | `hardware_concurrency()` |
| `wait_idle()`이 GPU 한계에서 오래 멈추면? | 1-D3 | 기다리는 쪽이 옳다. 필요하면 present_wait 검토 |
| Event payload 64B를 넘으면? | 1-C1 | 핸들로 전달한다 |
| MPSC를 lock-free로 바꿀 때? | 1-C5 · 1-D3 | 경합이 측정되면 Phase 2 후보 |
| 디스패치를 전용 스레드로 옮길 때? | 게이트 C | 메인 스레드 프레임 경계 |
| 렌더 스레드를 넣을 때? | `scene_world.md` §6 | 넣지 않는다 |

---

## 10. 참고 문서

- [`CODING_RULES.md`](../CODING_RULES.md) §9 · §10 · §16 · §17
- [`frame.md`](../design/frame.md) §5 · §6 · §9
- [`scene_world.md`](../design/scene_world.md) §1–§2 · §5–§6
