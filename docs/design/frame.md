# 프레임 수명 설계 결정사항

프레임 경계를 누가 정의하고, GPU 자원이 언제 살아 있는지를 정한다.

코드와의 불일치가 있다면 문서를 고치는 것을 우선한다.

---

## 1. 문제

프레임 하나에는 두 개의 독립적인 주기가 있다.

```text
CPU 주기 : 한 프레임을 얼마나 빨리 준비하는가
GPU 주기 : swapchain image가 언제 사용 가능해지는가
```

이 둘을 어떻게 맞추느냐가 프레임 수장의 전부다. 그래서 **프레임 경계를 누가 정의하는가**를
먼저 정해야 한다. 이 결정 없이는 frame allocator 위치, 리소스 파괴 시점,
동기화 전략이 모두 떠 있다.

## 2. 결정

**`nxt_graphics`가 프레임을 정의하고, `nxt_engine`이 프레임을 순회한다.**

```text
nxt_engine.frame_loop
    │
    ├─ graphics.swapchain.acquireNextImage()   → 다음 프레임 시작
    │
    ├─ renderer.record(encoder, target)         → 무엇을 그릴지
    │
    ├─ graphics.submit(encoder)                → GPU에 넘김
    │
    └─ graphics.swapchain.present()            → 화면에 표시
```

`graphics`는 **무엇을** 한다. `engine`은 **언제** 하는지 정한다.
`renderer`는 **무엇을 그릴지**만 결정하고, 그 과정에서 GPU 상태를 직접 다룰 수 없다.

## 3. 각 계층의 책임

| 계층 | 책임 | 모르는 것 |
|---|---|---|
| `platform` | `HWND` 제공, 이벤트 폴링, 창 상태 | GPU, 프레임 |
| `graphics` | acquire, submit, present, 동기화 | 씬, 렌더 순서 |
| `renderer` | pass를 나열하고 커맨드 버퍼에 기록 | Vulkan, 동기화 |
| `engine` | 위 세 호출의 순서를 결정 | GPU 드라이버 내부 |

이 결정이 기존에 없던 두 모듈의 경계를 갈라 놓는다.

```text
graphics/runtime   디바이스 초기화와 프레임 제출 관리   → "무엇을"
engine/frame_loop  프레임 루프                          → "언제"
```

기존 문서에서 이 둘의 경계가 정의되지 않은 상태였다. 이제 `present`가
`graphics`에 속한다는 사실에서 두 책임이 자연스럽게 갈린다.

## 4. 프레임 수명 흐름

```text
1. acquire        다음 swapchain image를 요청한다.
                  이 시점부터 GPU가 그 image에 쓰기 시작할 수 있다.

2. record         renderer가 커맨드 버퍼에 그릴 것을 기록한다.
                  이 시점의 모든 GPU 접근은 방금 얻은 image다.

3. submit         기록한 커맨드 버퍼를 GPU 큐에 올린다.

4. present        화면에 제시하고 fence를 신호시킨다.

5. wait           프레임이 필요 이상 앞서가지 않도록 fence를 기다린다.
```

**acquire에서 얻은 image를 present 전까지 다른 곳에서 쓰면 안 된다.**
이 구간이 그 image의 수명이다. 5번 이후로 넘어간 image는 다음 프레임에서 재사용된다.

## 5. 프레임 페이싱 — 백프레시

GPU가 CPU보다 빠르면 큐가 무한히 쌓인다. 프레임마다 fence를 기다려
**CPU가 GPU보다 앞서가지 못하게** 만든다.

```text
지원하는 프레임 수 = swapchain image 개수
```

**프레임 레인 수 = swapchain image 개수. 기본값은 3(트리플 버퍼링)이다.**

swapchain의 `minImageCount`가 상한을 결정한다. Khronos 실측 기준으로
**더블 버퍼링은 VSync를 한 번 놓치면 프레임률이 절반으로 떨어진다.** 프레이젠테이션
엔진이 다음 VSync까지 이미 완료된 이미지를 돌려줘야 GPU가 멈추지 않는데, 더블
버퍼링에는 대기 중인 이미지가 없기 때문이다. 트riple 버퍼링은 이 cliff를 없앤다.

present mode는 **FIFO**를 기본값으로 한다. MAILBOX는 지연이 낮을 때만 필요하다.

### present용 semaphore의 색인 기준

**프레임 인덱스가 아니라 swapchain image 인덱스로 색인한다.**

Vulkan 명세상 `vkQueuePresentKHR`는 fence나 semaphore를 신호하지 않는다. 따라서
`vkQueueSubmit`의 fence를 기다렸다고 present가 끝났다고 보장할 수 없다.
명세가 인정한 유일한 안전한 방법은 "`vkAcquireNextImageKHR`로 얻은 이미지 인덱스로
색인된 semaphore를 사용"하는 것이다. 이미지 인덱스를 얻고 그 semaphore 또는 fence를
기다리면, 그 이미지를 쓴 이전 present가 완료되었다는 것이 보장된다.

```text
acquire_semaphores[framesInFlight]        ← 프레임 인덱스
submit_semaphores[swapchainImageCount]    ← 이미지 인덱스
```

두 배열을 같은 인덱스 체계로 잡으면 동시 사용 오류를 만든다.

**GPU가 완전히 멈추면(wait가 무한정 걸리면) 어떻게 할지는 Phase 1에서
실측으로 결정한다.** 지금은 기다리는 쪽이 옳다고 가정한다. 정확한 페이싱이 필요해지면
`VK_KHR_present_wait` / `VK_KHR_present_wait2`를 검토한다.

## 6. 리사이즈

창 크기가 바뀌면 swapchain이 무효가 된다. 순서는 고정이다.

```text
1. 크기 변화 감지
2. 이전 프레임의 fence를 모두 기다린다      ← GPU가 리소스를 쓰는 중일 수 있다
3. 파이프라인 제거
4. swapchain 재생성
5. 모든 GPU 리소스를 새 크기에 맞춰 재생성
6. 파이프라인 재생성
```

**2번을 빠뜨리면 GPU가 파괴된 리소스에 접근한다.** 리사이즈를 빠르게 반복하면
재현되는 버그가 되므로, 리사이즈 경로에는 반드시 테스트를 둔다.

## 7. 프레임 스크래치 메모리 위치

기존 문서(`phase0.md`의 "FrameAllocator는 Core에 두지 않는다")가 미해결로 남겼던 항목이다.
여기서 닫는다.

**프레임 allocator는 `nxt_graphics`에 둔다.**

```text
nxt_core/memory/     SystemAllocator, Pool, Arena — 주기 무관 범용 메모리
nxt_graphics/        프레임 스크래치 — present가 프레임을 정의하므로 여기가 소유자
```

이유는 하나다. **프레임의 끝을 정의하는 모듈이 프레임 메모리를 소유해야 한다.**
`present`를 호출하는 쪽이 프레임 경계를 알기 때문이다. `core`에 두면
프레임 수명에 관한 지식이 가장 아래 계층에 새고 들어간다.

Phase 0에서 프레임 스크래치는 필요하지 않다. Phase 3(다중 패스)에서 GPU 상태
up/down transition용으로 도입된다. 그때 `Pool`과 `Arena`의 첫 실제 소비자가 된다.

## 8. 동기화 기본값

| 수단 | Phase 0 쓰임 |
|---|---|
| Fence (CPU 대기) | 매 프레임 백프레시, 리소스 파괴, 리사이즈 정리 |
| Semaphore (GPU 대기) | acquire → submit, submit → present |
| Pipeline barrier | `submit`의 암묵적 대기(hand-off barrier)로 충분하다 |

acquire/present용 semaphore는 §5의 규칙대로 **이미지 인덱스 기준**으로 관리한다.
파이프라인 간 GPU 동기화를 위한 semaphore와 명시적 barrier는 Phase 3의 다중 패스에서
도입한다.

## 9. 리소스 파괴 — fence 기반 지연 파괴

**트리플 버퍼링을 쓰면 GPU가 여러 프레임을 동시에 처리 중이다.** 지금 파괴하면
그중 하나가 아직 접근한다. 즉각 파괴는 use-after-free이고, 고치지 않는다.

```text
destroy(buffer)
    ↓
마지막으로 사용한 프레임 번호를 기록하고 지연 목록에 넣는다
    ↓
해당 프레임의 fence가 신호될 때까지 보류
    ↓
실제 vkDestroyBuffer 호출
```

Filament의 `Fence::waitAndDestroy()`와 Unreal의 `FlushPendingDeleteRHIResources`가
같은 구조다.

### 처리 시점

```text
프레임 N 시작
  1. 프레임 N - framesInFlight 의 fence 대기        ← GPU를 이 프레임까지 앞세운다
  2. 그 fence가 신호되었으므로, 그 프레임이 마지막으로 쓴 리소스를 파괴해도 안전하다
  3. 지연 목록에서 fence가 완료된 항목만 실제 파괴
  4. acquire → record → submit → present
```

**1번이 2번의 근거다.** fence를 기다린 뒤에야 "이 리소스는 더 이상 쓰이지 않는다"를
말할 수 있다. 순서를 바꾸면 파괴 직전에 GPU가 여전히 쓰는 리소스를 지우게 된다.

### 의존성 순서

**높은 수준의 리소스를 낮은 수준보다 먼저 파괴한다.**

```text
Pipeline, Sampler  →  Buffer, Image
```

파이프라인이 인스턴스화될 때 버퍼를 참조하므로, 버퍼가 먼저 파괴되면
파이프라인이 죽은 리소스를 가리킨다. Filament도 종료 시 같은 순서를 강제한다.

이 순서를 지키려면 "무엇이 무엇을 참조하는지" 알아야 하므로,
**Phase 0에서는 파이프라인을 캐시하지 않고 하나만 만든다.** 캐시가 필요해지는 시점에
의존성 그래프를 넣는다.

### 리사이즈와 파괴

리사이즈는 §6의 순서를 따른다. 2번의 fence 대기가 곧 위 절차의 강제 실행이다.
파괴를 별도 경로로 만들지 않고 같은 지연 파괴 경로를 쓴다.

## 10. 핵심 원칙

1. 프레임을 정의하는 모듈은 `nxt_graphics`다. `engine`은 순회만 한다.
2. `acquire`에서 얻은 image는 `present` 전까지 다른 곳에서 쓰지 않는다.
3. 버퍼링은 트리플이 기본이다. 더블 버퍼링은 VSync를 놓치면 프레임률이 절반으로 떨어진다.
4. present용 semaphore는 **이미지 인덱스**로 색인한다. `vkQueuePresentKHR`는 fence를 신호하지 않는다.
5. 리소스는 fence 대기 없이 파괴하지 않는다. 지연 목록을 거쳐 파괴한다.
6. 높은 수준의 리소스를 낮은 수준보다 먼저 파괴한다.
7. 리사이즈에서는 GPU 사용이 끝난 뒤에 리소스를 파괴한다.
8. 프레임 allocator는 `nxt_graphics`에 둔다. `core`는 프레임 수명을 모른다.
9. 파이프라인 간 동기화는 Phase 3에서 넣는다. 필요한 게 없는데 넣으면 규칙 16 위반이다.
10. `present`가 두 모듈을 가르는 기준이 된다: 그래픽스는 무엇을, 엔진은 언제를 한다.