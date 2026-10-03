# NXT Coding Rules

## 1. 기본 원칙

NXT는 **구조에는 엄격하고, 구현에는 관대하게** 설계한다.

- 모듈 간 책임과 의존성은 명확하게 유지한다.
- ownership과 lifetime을 명확하게 한다.
- 불필요한 추상화와 범용화를 피한다.
- 요구사항이 발생하기 전에 미래의 확장성을 구현하지 않는다.
- 구체적인 사용 사례가 여러 개 존재하거나 명확한 확장 요구가 생겼을 때 추상화를 도입한다.
- 성능 최적화는 측정된 병목을 기준으로 수행한다.
- 단순한 구현으로 충분하다면 단순한 구현을 우선한다.

> 규칙을 지키기 위해 코드가 복잡해져서는 안 된다.


## 2. 의존성

`nxt_core`는 C++ 표준 라이브러리를 기본 의존성으로 사용한다.

```text
nxt_core
   ↓
C++ Standard Library
```

Core는 특정 외부 라이브러리에 직접 의존하지 않는다.

다음 라이브러리는 Core에서 직접 include하지 않는다.

- `glm`
- `spdlog`
- `fmt`
- `EASTL`
- `stb`
- `windows.h`
- `pthread.h`
- `unistd.h`

외부 라이브러리가 필요한 경우 상위 모듈 또는 별도의 adapter/backend에서 연결한다.

`nxt_math`는 별도의 foundation 모듈로 유지한다.

Phase 0에서는 `nxt_core`가 `nxt_math`를 직접 참조하지 않는다.


## 3. 플랫폼 독립성

Core는 OS API를 직접 호출하지 않는다.

플랫폼에 종속적인 기능이 필요한 경우 다음과 같은 방향으로 의존성을 구성한다.

```text
nxt_core
    ↓
interface
    ↓
platform/
    ↓
OS API
```

예:

- Windows → `windows.h`
- Linux → `unistd.h`
- Thread backend
- Window backend
- OS event handling
- File system backend

단, 실제로 플랫폼 추상화가 필요한 시점까지 불필요한 interface를 미리 만들지 않는다.


## 4. Header

모든 public header는 self-contained해야 한다.

다음과 같이 include했을 때 필요한 선언을 스스로 제공해야 한다.

```cpp
#include <nxt/core/...>
```

기본 규칙:

- `#pragma once` 사용
- 필요한 header는 직접 include
- 우연한 transitive include에 의존하지 않는다.
- 불필요한 include는 추가하지 않는다.


## 5. 예외

Core는 exception 없이 빌드할 수 있어야 한다.

다음 기능은 Core 코드에서 사용하지 않는다.

- `try`
- `catch`
- `throw`

`-fno-exceptions` 환경에서도 빌드 가능하도록 작성한다.

복구 가능한 실패는 반환값으로 처리한다.

```cpp
bool destroy(Handle handle);
```

프로그래머 오류나 불변식 위반은 `NXT_ASSERT`를 사용한다.

단, 모든 실패를 무조건 별도의 `Error` 타입으로 감싸지는 않는다.

간단한 작업은 다음과 같이 단순한 반환값을 사용할 수 있다.

```cpp
bool contains(Handle handle);
```

복잡한 오류 정보가 실제로 필요한 경우에만 별도의 Error 타입을 도입한다.


## 6. 메모리

Hot path에서 불필요한 암묵적 allocation을 발생시키지 않는다.

가능하면 명시적인 ownership과 lifetime을 유지한다.

메모리 추상화가 필요한 경우 `Allocator`, `Pool`, `Arena` 등의 구조를 사용한다.

예:

```text
core/memory/
    allocator.hpp
    pool.hpp
    arena.hpp
```

단, 모든 자료구조에 무조건 Allocator를 주입하지 않는다.

간단한 Core 자료구조에서 `std::vector` 등의 표준 컨테이너를 사용하는 것은 허용한다.

예외:

- 렌더링 hot path
- 빈번한 frame allocation
- 명확한 메모리 lifetime 관리가 필요한 경우
- 성능 측정 결과 allocation이 병목으로 확인된 경우

이러한 경우 명시적인 memory resource 또는 NXT memory abstraction을 사용한다.


## 7. 정수 타입

Public API에서는 고정 폭 정수를 사용한다.

```cpp
std::uint32_t
std::uint64_t
std::int32_t
std::int64_t
```

예:

```cpp
std::uint32_t index;
std::uint64_t timestamp;
```

단순한 local variable이나 STL API와의 상호작용 등에서는 필요 이상으로 타입을 강제하지 않는다.


## 8. Namespace

기본 namespace:

```cpp
nxt::
```

하위 namespace는 기능 영역과 일치시킨다.

예:

```cpp
nxt::jobs
nxt::mem
nxt::event
nxt::diag
nxt::concurrency
```

namespace는 디렉터리 구조와 가능한 한 일관성을 유지한다.


## 9. Thread Safety

Concurrent public type은 producer / consumer / synchronization contract를 명확하게 한다.

예:

| Type | Producer | Consumer | 보장 |
|---|---:|---:|---|
| `SpscQueue` | 1 | 1 | Lock-free |
| `MpscQueue` | N | 1 | Lock-free |
| `WorkStealingDeque` | Owner 1 | Stealer N | Lock-free steal |

Thread-safe하지 않은 타입은 별도로 thread-safe하다고 가정하지 않는다.

Atomic ordering은 필요한 최소 수준으로 사용한다.

기본 원칙:

```text
relaxed
    ↓
acquire / release
    ↓
seq_cst
```

가능하면 `relaxed`를 사용하고, synchronization이 필요한 경우 `acquire/release`를 사용한다.

`seq_cst`는 실제로 필요한 경우에만 사용한다.

특정 CPU의 메모리 모델을 전제로 코드를 작성하지 않는다.

Concurrent type의 구현이 복잡한 경우 코드 또는 문서에 synchronization contract를 명시한다.


## 10. Logging / Diagnostics

Core는 직접 stdout/stderr로 출력하지 않는다.

다음과 같은 직접 출력은 사용하지 않는다.

```cpp
std::cout
std::cerr
printf
```

Diagnostics 또는 logging abstraction을 통해 외부 sink로 전달한다.

예:

```cpp
nxt::diag::log(...);
```

단, 단순한 로컬 디버깅 과정에서 일시적인 출력 코드를 사용하는 것까지 엄격하게 금지하지 않는다.

Debugging 코드가 실제 코드에 남는 경우에는 적절한 diagnostics abstraction으로 교체한다.


## 11. Assertion

프로그램의 불변식이나 프로그래머 오류는 assertion으로 검증한다.

예:

```cpp
NXT_ASSERT(handle.valid());
NXT_ASSERT(index < size);
```

Assertion은 일반적인 runtime error handling의 대체 수단으로 사용하지 않는다.

즉:

```text
예상 가능한 실패
    → 반환값 / 상태

프로그래머 오류 / 불변식 위반
    → assertion
```


## 12. Ownership과 Lifetime

모든 resource는 명확한 owner를 가져야 한다.

가능하면 다음 사항을 코드에서 명확하게 알 수 있어야 한다.

- 누가 생성하는가?
- 누가 소유하는가?
- 누가 파괴하는가?
- lifetime은 얼마나 지속되는가?
- thread 간 공유가 가능한가?

불필요한 raw pointer ownership을 사용하지 않는다.

다만 non-owning reference나 pointer를 표현하는 용도로 raw pointer를 사용하는 것은 허용한다.


## 13. API 설계

Public API는 필요한 기능만 노출한다.

구현 세부사항은 가능한 한 private으로 유지한다.

다음과 같은 코드를 요구사항 없이 미리 추가하지 않는다.

- 불필요한 template parameter
- 불필요한 allocator parameter
- serialization API
- reflection
- generic callback system
- 과도한 trait system
- 사용하지 않는 configuration structure

예를 들어 현재 요구사항이 다음과 같다면:

```cpp
bool contains(Handle handle);
```

단순히 미래의 확장을 이유로 다음과 같이 만들지 않는다.

```cpp
Result<bool, ErrorCode, Diagnostics, Context>
```

실제 요구사항이 생긴 경우에만 API를 확장한다.


## 14. Template

Template은 실제로 필요한 경우 사용한다.

다음과 같은 경우 template 사용을 우선 고려한다.

- 타입에 따라 동작이 달라지는 자료구조
- compile-time abstraction
- zero-cost abstraction
- 명확한 code reuse

반대로 단순히 "나중에 다른 타입을 사용할 수도 있으니까"라는 이유만으로 template화하지 않는다.


## 15. Data Structure

자료구조는 먼저 가장 단순한 구현을 선택한다.

예:

```cpp
std::vector
std::array
std::span
std::optional
```

등의 표준 자료구조로 충분하다면 직접 구현하지 않는다.

직접 자료구조를 구현해야 하는 경우는 다음과 같다.

- 성능 요구
- 특정 concurrency model
- 특수한 memory layout
- 명확한 ownership/lifetime 요구
- 표준 자료구조로 표현하기 어려운 엔진 특화 요구

예:

```text
std::vector
    ↓
성능/수명/메모리 요구 발생
    ↓
Pool / Arena / Custom Container
```


## 16. Performance

성능을 추측해서 최적화하지 않는다.

기본 순서:

```text
Simple implementation
        ↓
Measure
        ↓
Identify bottleneck
        ↓
Optimize
        ↓
Measure again
```

특히 다음을 이유 없이 적용하지 않는다.

- lock-free
- custom allocator
- object pool
- intrusive container
- SIMD
- custom container
- complex caching
- aggressive memory ordering

필요성이 측정되거나 명확한 설계 요구가 있을 때 도입한다.


## 17. Concurrency

Concurrency는 필요한 곳에서만 사용한다.

단일 thread로 충분한 기능을 억지로 concurrent하게 만들지 않는다.

Concurrency를 도입할 경우 다음을 먼저 정의한다.

1. 누가 데이터를 생성하는가?
2. 누가 데이터를 소비하는가?
3. ownership은 누구에게 있는가?
4. synchronization은 어디에서 발생하는가?
5. lifetime은 어떻게 보장되는가?

그 후 mutex, atomic, queue, lock-free structure 등의 방법을 선택한다.

Lock-free를 사용하는 것 자체를 목표로 하지 않는다.


## 18. 파일 및 디렉터리 구조

디렉터리는 기능적 책임을 기준으로 나눈다.

예:

```text
src/nxt/
├── core/
│   ├── handle/
│   ├── memory/
│   ├── concurrency/
│   └── ...
├── jobs/
├── event/
├── renderer/
│   └── memory/
├── platform/
│   └── backends/
│       └── windows/
└── ...
```

파일을 추가할 때는 먼저 다음을 판단한다.

> "이 코드의 책임은 어디에 있는가?"

서로 다른 책임을 하나의 파일에 무리하게 넣지 않는다.

반대로 단순한 코드까지 지나치게 세분화하지 않는다.


## 19. Comments

코드가 무엇을 하는지 그대로 설명하는 주석은 최소화한다.

다음과 같은 경우에는 주석을 작성한다.

- 왜 이렇게 구현했는지
- 특정 memory ordering을 사용하는 이유
- ownership 규칙
- lifetime 제약
- 플랫폼 특이사항
- 알고리즘의 중요한 불변식
- 외부 API의 특이한 동작에 대한 workaround

예:

```cpp
// acquire is required here because the consumer
// must observe the producer's published payload.
```

단순한 코드 설명은 코드 자체로 표현한다.


## 20. Simplicity

NXT의 모든 설계에서 다음 원칙을 우선한다.

> **필요한 만큼만 만든다.**

다음 상황에서는 추상화를 추가하지 않는다.

- 아직 사용 사례가 하나뿐인 경우
- 확장 가능성이 단순한 추측에 불과한 경우
- 추상화가 현재 코드를 더 복잡하게 만드는 경우
- 성능상의 이점이 측정되지 않은 경우

다음과 같은 요구가 발생하면 추상화를 고려한다.

- 동일한 패턴이 여러 곳에서 반복되는 경우
- 서로 다른 구현이 실제로 필요한 경우
- 모듈 간 의존성을 끊어야 하는 경우
- 성능 또는 lifetime 요구가 명확해진 경우
- 플랫폼 차이를 숨겨야 하는 경우


## 21. 핵심 원칙

```text
Architecture
    → Strict

Ownership
    → Clear

Module Boundary
    → Strict

Implementation
    → Simple

Abstraction
    → As Needed

Optimization
    → Measure First
```

NXT는 미래의 모든 요구사항을 미리 해결하는 것이 아니라,
**현재 필요한 요구사항을 단순하게 해결하고 실제 요구가 발생했을 때 확장한다.**
