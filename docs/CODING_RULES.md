# NXT Coding Rules

이 문서는 NXT-Engine **전 계층**에 적용되는 코딩 규약이다.

- 제1부 — 모든 계층에 공통으로 적용되는 일반 규칙
- 제2부 — `core` / `platform` 등 계층별 규칙

규칙은 방향을 제시하기 위한 것이지, 규칙을 지키려고 코드가 복잡해지는 것을
정당화하지 않는다. 판단이 갈리면 단순한 쪽을 택한다.

```text
구조에는 일관되게, 구현에는 유연하게.
```

## 목차

- [제1부 — 일반 규칙](#제1부--일반-규칙)
  - [1. 기본 원칙](#1-기본-원칙)
  - [2. 언어 및 표준](#2-언어-및-표준)
  - [3. 포맷팅](#3-포맷팅)
  - [4. 명명 규약](#4-명명-규약)
  - [5. 헤더](#5-헤더)
  - [6. 네임스페이스](#6-네임스페이스)
  - [7. 주석 및 문서화](#7-주석-및-문서화)
  - [8. 정수 타입](#8-정수-타입)
  - [9. Ownership과 Lifetime](#9-ownership과-lifetime)
  - [10. API 설계](#10-api-설계)
  - [11. Template](#11-template)
  - [12. Data Structure](#12-data-structure)
  - [13. 예외](#13-예외)
  - [14. Assertion](#14-assertion)
  - [15. 메모리](#15-메모리)
  - [16. 성능](#16-성능)
  - [17. Concurrency / Thread Safety](#17-concurrency--thread-safety)
  - [18. Process I/O](#18-process-io)
  - [19. 파일 및 디렉터리 구조](#19-파일-및-디렉터리-구조)
  - [20. Simplicity](#20-simplicity)
- [제2부 — 계층별 규칙](#제2부--계층별-규칙)
  - [core (`nxt_core`)](#core-nxt_core)
  - [platform (`nxt_platform`)](#platform-nxt_platform)
  - [engine / renderer / graphics / assets](#engine--renderer--graphics--assets)
- [핵심 원칙](#핵심-원칙)

---

# 제1부 — 일반 규칙

## 1. 기본 원칙

- 모듈의 책임과 의존성은 명확하게 유지한다.
- ownership과 lifetime을 명확하게 한다.
- 불필요한 추상화와 범용화를 피한다.
- 단순한 구현으로 충분하면 단순한 구현을 우선한다.
- 성능 최적화는 측정된 병목을 기준으로 한다.

## 2. 언어 및 표준

- **C++20**을 기준으로 한다.
- 경고는 켜고 빌드한다.
  - MSVC: `/W4`
  - GCC / Clang: `-Wall -Wextra`
- 경고는 가능하면 제거한다. 불가피한 경우에만 명시적으로 억제한다.

## 3. 포맷팅

포맷은 `.clang-format`을 따른다. 취향보다 포맷터 결과를 신뢰한다.

```text
LLVM 기반 · 4 space · 120 column
pointer left · brace attach · include regroup
```

## 4. 명명 규약

| 대상 | 규칙 | 예 |
|---|---|---|
| 타입 | PascalCase | `Handle`, `LogManager` |
| 함수 / 메서드 | camelCase | `valid()`, `reportAssertFailure()` |
| 상수 | `k` + PascalCase | `kInvalidIndex` |
| 멤버 변수 | trailing underscore | `index_`, `categoryLevels_` |
| 매크로 | `NXT_` + SCREAMING_SNAKE | `NXT_ASSERT`, `NXT_LOG_INFO` |
| 파일 | snake_case | `handle.hpp`, `spsc_queue.cpp` |
| namespace | 소문자, 디렉터리와 일치 | `nxt::core::handle` |

## 5. 헤더

모든 public header는 self-contained해야 한다.

- `#pragma once`를 사용한다.
- 필요한 header는 직접 include한다.
- 우연한 transitive include에 의존하지 않는다.
- 불필요한 include는 추가하지 않는다.

```cpp
#include <nxt/core/...>   // 이것만으로 필요한 선언이 제공되어야 한다.
```

## 6. 네임스페이스

기본 namespace는 `nxt::`이며, 하위 namespace는 기능 영역과 일치시킨다.

```cpp
nxt::core::handle
nxt::platform::windows
nxt::renderer
```

namespace는 디렉터리 구조와 가능한 한 일관성을 유지한다.

## 7. 주석 및 문서화

- public API는 한국어 Doxygen 주석을 사용한다.
  ```cpp
  /// @brief 유효한 슬롯을 가리키는지 여부를 반환한다.
  ```
- 내부 구현 주석은 "무엇"보다 **"왜"**에 집중한다.
- 다음은 값이 있을 때 기록한다.
  - 특정 memory ordering을 쓰는 이유
  - ownership / lifetime 제약
  - 플랫폼 특이사항이나 workaround
  - 알고리즘의 중요한 불변식

## 8. 정수 타입

public API에서는 고정 폭 정수를 사용한다.

```cpp
std::uint32_t
std::uint64_t
std::int32_t
std::int64_t
```

local variable이나 STL API와의 상호작용에서는 타입을 필요 이상으로 강제하지 않는다.

## 9. Ownership과 Lifetime

모든 resource는 명확한 owner를 가진다. 코드만 보고 다음을 알 수 있어야 한다.

- 누가 생성 / 소유 / 파괴하는가?
- lifetime은 얼마나 지속되는가?
- thread 간 공유가 가능한가?

owning raw pointer는 피한다. non-owning 참조를 표현하는 용도의 raw pointer는 허용한다.

## 10. API 설계

Public API는 필요한 기능만 노출하고, 구현 세부사항은 private으로 유지한다.

요구사항이 없는데 다음을 미리 추가하지 않는다.

- 불필요한 template / allocator parameter
- serialization / reflection
- generic callback system
- 과도한 trait system
- 사용하지 않는 configuration structure

```cpp
bool contains(Handle handle);   // 충분하다.
// Result<bool, ErrorCode, Diagnostics, Context>   // 미리 만들지 않는다.
```

실제 요구가 생긴 경우에만 확장한다.

## 11. Template

실제로 필요한 경우에 사용한다.

- 타입에 따라 동작이 달라지는 자료구조
- compile-time abstraction / zero-cost abstraction
- 명확한 code reuse

"나중에 다른 타입을 쓸 수도 있으니까"라는 이유만으로 template화하지 않는다.

## 12. Data Structure

먼저 가장 단순한 구현을 선택한다.

```cpp
std::vector
std::array
std::span
std::optional
```

표준 자료구조로 충분하면 직접 구현하지 않는다. 다음 요구가 있을 때 직접 구현한다.

- 성능 요구
- 특정 concurrency model
- 특수한 memory layout
- 명확한 ownership / lifetime 요구

## 13. 예외

프로젝트는 exception 없이 빌드 가능한 것을 목표로 한다.

- 복구 가능한 실패는 반환값으로 처리한다.
  ```cpp
  bool destroy(Handle handle);
  ```
- 프로그래머 오류나 불변식 위반은 assertion으로 처리한다.
- 모든 실패를 무조건 별도의 `Error` 타입으로 감싸지 않는다. 복잡한 오류
  정보가 실제로 필요할 때만 도입한다.

## 14. Assertion

프로그래머 오류와 불변식은 `NXT_ASSERT` / `NXT_VERIFY`로 검증한다.

```text
예상 가능한 실패      → 반환값 / 상태
프로그래머 오류       → assertion
```

Assertion은 일반적인 runtime error handling의 대체 수단으로 사용하지 않는다.

## 15. 메모리

- hot path에서 불필요한 암묵적 allocation을 만들지 않는다.
- ownership과 lifetime을 가능한 한 명시적으로 유지한다.
- 메모리 추상화가 필요하면 `Allocator`, `Pool`, `Arena`를 사용한다.
- 모든 자료구조에 allocator를 주입하지는 않는다. 단순한 자료구조에서
  `std::vector` 같은 표준 컨테이너를 쓰는 것은 허용한다.

다음 경우는 명시적인 memory resource를 우선 고려한다.

- 렌더링 hot path
- 빈번한 frame allocation
- 명확한 memory lifetime 관리가 필요한 경우
- 측정 결과 allocation이 병목으로 확인된 경우

## 16. 성능

성능을 추측해서 최적화하지 않는다.

```text
Simple implementation
        ↓
Measure → Identify bottleneck → Optimize → Measure again
```

다음을 이유 없이 적용하지 않는다.

- lock-free, custom allocator, object pool
- intrusive container, custom container
- SIMD, complex caching, aggressive memory ordering

필요성이 측정되거나 설계 요구가 명확할 때 도입한다.

## 17. Concurrency / Thread Safety

- 필요한 곳에만 사용한다. 단일 thread로 충분한 기능을 억지로 concurrent하게
  만들지 않는다.
- 도입 전에 다음을 먼저 정의한다.
  1. 누가 생성 / 소비하는가?
  2. ownership은 누구에게 있는가?
  3. synchronization은 어디서 일어나는가?
  4. lifetime은 어떻게 보장되는가?
- concurrent public type은 producer / consumer / synchronization contract를
  명확히 한다.
- atomic ordering은 필요한 최소 수준을 쓴다.

  ```text
  relaxed → acquire / release → seq_cst
  ```

- 특정 CPU의 메모리 모델을 전제로 코드를 작성하지 않는다.
- thread-safe하지 않은 타입을 thread-safe하다고 가정하지 않는다.

## 18. Process I/O

표준 입출력에 직접 쓰지 않고, 상위 레이어가 소유하는 경로로 넘긴다.

```cpp
std::cout / std::cerr / printf
```

단, 실패 경로는 예외다. 종료 조건을 보고하는 기록은 그 경로에 의존하면 안 되며,
직접 기록한 뒤 flush하고 종료한다.

로컬 디버깅용 임시 출력은 허용하지만, 실제 코드에 남기지 않는다.

## 19. 파일 및 디렉터리 구조

디렉터리는 기능적 책임을 기준으로 나눈다.

파일을 추가할 때 먼저 다음을 판단한다.

> "이 코드의 책임은 어디에 있는가?"

서로 다른 책임을 한 파일에 무리하게 넣지 않는다. 반대로 단순한 코드까지
지나치게 세분화하지 않는다.

## 20. Simplicity

> **필요한 만큼만 만든다.**

다음 경우에는 추상화를 추가하지 않는다.

- 사용 사례가 아직 하나뿐인 경우
- 확장 가능성이 단순한 추측에 불과한 경우
- 추상화가 현재 코드를 더 복잡하게 만드는 경우

반복되는 패턴, 실제로 다른 구현의 필요, 모듈 의존성 분리, 성능·lifetime
요구, 플랫폼 차이 은폐가 확인되면 추상화를 고려한다.

---

# 제2부 — 계층별 규칙

## core (`nxt_core`)

Core는 가장 아래 계층이며, 어떤 외부 라이브러리에도 종속되지 않도록 유지한다.

- **의존성**: C++ 표준 라이브러리만 사용한다. 다음은 Core에서 직접 include하지 않는다.
  - `glm` · `spdlog` · `fmt` · `EASTL` · `stb`
  - `windows.h` · `pthread.h` · `unistd.h`
- **플랫폼 독립성**: OS API를 직접 호출하지 않는다. 플랫폼 기능이 필요하면
  interface를 두고 `platform` backend가 구현한다.

  ```text
  nxt_core → interface → platform/ → OS API
  ```

  단, 실제로 필요한 시점까지 불필요한 interface를 미리 만들지 않는다.
- **예외**: exception 없이(`-fno-exceptions`) 빌드 가능해야 한다. `try` /
  `catch` / `throw`를 사용하지 않는다.
- **Process I/O**: Core는 프로세스 입출력을 소유하지 않는다.
- **namespace**: `nxt::core::...`

## platform (`nxt_platform`)

- `nxt_core`에 의존한다.
- OS API와 외부 라이브러리 연결을 담당한다.
- backend는 `platform/backends/<os>/`에 둔다.
- 로깅 sink처럼 실제 출력을 수행하는 구현을 소유한다.
- **namespace**: `nxt::platform::...`

## engine / renderer / graphics / assets

- 상위 계층은 하위 계층에 의존한다. 역방향 의존은 만들지 않는다.
- 외부 라이브러리는 상위 계층 또는 backend에서 연결한다.
- 세부 규칙은 각 계층 문서와 `docs/roadmap/`을 따른다.

---

# 핵심 원칙

```text
Architecture     → Consistent
Ownership        → Clear
Module Boundary  → Clear
Implementation   → Simple
Abstraction      → As Needed
Optimization     → Measure First
```

NXT는 미래의 모든 요구사항을 미리 해결하지 않는다. **현재 필요한 요구사항을
단순하게 해결하고, 실제 요구가 발생했을 때 확장한다.**
