# Logging 설계 결정사항

NXT 로깅 시스템을 구현하거나 검토할 때 아래의 설계 결정사항을 기준으로 삼는다.

코드와의 불일치가 있다면 문서를 고치는 것을 우선한다.

---

## 1. 기본 구조

```text
NXT_LOG_*
    ↓
nxt::log::log()
    ↓
LogManager
    ↓
LogSink (여러 개)
    ↓
Console / File
```

- `LogManager`는 중앙 로깅 관리자다.
- `LogManager` 자체는 Singleton으로 구현하지 않는다.
- 전역 접근은 별도의 free function을 통해 제공한다.
- 실제 출력은 `LogSink`가 담당한다.
- Core 로깅 시스템은 로그가 어디로 출력되는지 알지 않는다.

## 2. Namespace

```cpp
namespace nxt::log
```

출력 대상인 Sink는 platform 계층에 두므로 다른 namespace를 쓴다.

```cpp
namespace nxt::platform::win32::log
```

## 3. LogLevel

6단계로 정의하며 값이 클수록 심각도가 높다.

```cpp
enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warn, Error, Fatal };
```

출력 여부는 다음 규칙으로 판단한다.

```cpp
level >= effectiveLevel
```

## 4. LogCategory

카테고리는 **계층**과 **하위 시스템** 두 축으로 이루어진다.

```cpp
enum class LogLayer : std::uint8_t { Core, Platform, Graphics, Renderer, Assets, Engine };

enum class LogSubsystem : std::uint8_t {
    None,        // 하위 디렉터리가 없는 모듈 루트 파일
    Diagnostics, Handle, Time, Memory, Containers, Concurrency, Jobs, Event,   // core
    Window, Input, Filesystem, Win32,                                         // platform
    Rhi, Vulkan,                                                              // graphics
    Graph, Material, Mesh, Passes, World,                                     // renderer
    Types, Loaders,                                                           // assets
    FrameLoop, Scene,                                                         // engine
    LogSubsystemCount
};

struct LogCategory {
    LogLayer layer{LogLayer::Core};
    LogSubsystem subsystem{LogSubsystem::None};
    auto operator<=>(const LogCategory&) const noexcept = default;
};
```

Layer는 `src/nxt` 아래의 최상위 디렉터리에, Subsystem은 모듈 바로 아래의
디렉터리에 대응한다. 백엔드도 하위 시스템 하나로 본다(`platform/backends/windows`
→ `Win32`, `graphics/backends/vulkan` → `Vulkan`).

Subsystem 이름은 계층 간에 겹치지 않는다. 그래서 카테고리별 level override
배열은 Subsystem 값만으로 색인해도 정확하다.

Category와 Level은 서로 독립이다. Category는 어디서, Level은 얼마나 심각한가.

### 표시 형태

두 축이 각각 독립된 대괄호 그룹이 된다.

```text
Platform / Win32   →  [Platform][Win32]
Engine  / None     →  [Engine]
```

하위 시스템이 없는 모듈 루트 파일(`engine/engine.cpp`, `renderer/renderer.cpp`,
앱 `main.cpp`)은 `None`을 쓴다.

### 매크로

호출부는 두 개의 이름만 준다. 구조체 조립은 매크로가 끝낸다.

```cpp
NXT_LOG_ERROR(Platform, Win32, "CreateWindowExW failed, error={}", error);
NXT_LOG_INFO(Engine, None, "Initializing NXT Engine");
```

## 5. Global Level + Category Override

기본값은 빌드 설정이 정한다. `LogManager::defaultLevel()`이 이를 담당한다.

```text
Debug    →  LogLevel::Debug
Release  →  LogLevel::Error
```

`NDEBUG`로 구분하며, `assert.hpp`와 같은 기준을 쓴다. 두 기준이 갈라지면
"이 로그는 왜 안 보이나"를 설명하기 어려워진다.

호출자가 `setLevel()`을 호출하면 이 값을 덮어쓴다. 배포 이진리는 조용해야 하므로
애플리케이션이 명시적으로 레벨을 올리지 않는 한 Release에서는 Error와 Fatal만 남는다.

카테고리는 override를 선택적으로 지정할 수 있다.

```cpp
std::array<std::optional<LogLevel>, static_cast<std::size_t>(LogSubsystem::LogSubsystemCount)> categoryLevels_;
```

override가 있으면 그 값을, 없으면 global을 쓴다.

```text
Global = Info, Renderer = Debug

Core              → Info
Platform          → Info
Platform/Win32    → Debug
Renderer          → Info
```

API:

```cpp
void setLevel(LogLevel level) noexcept;
LogLevel level() const noexcept;

void setCategoryLevel(LogCategory category, LogLevel level) noexcept;
void resetCategoryLevel(LogCategory category) noexcept;
LogLevel categoryLevel(LogCategory category) const noexcept;
```

`categoryLevel()`은 override가 없으면 global을 반환한다.

## 6. LogRecord

```cpp
struct LogRecord {
    using Clock = std::chrono::system_clock;

    LogLevel level;
    LogCategory category;
    std::string message;
    std::thread::id threadId;
    std::source_location location;
    Clock::time_point timestamp;
};
```

`message`는 반드시 `std::string`으로 소유한다. 향후 async logging으로 전환해도
문자열 수명이 레코드와 무관해지도록 하기 위해서다.

## 7. Timestamp

Sink가 출력하는 시점이 아니라 **LogRecord가 생성되는 시점**에 기록한다.

`std::chrono::system_clock`을 쓴다. 로그는 캘린더 시간이 필요하므로 monotonic
시계인 `nxt::time::Timestamp`를 쓸 수 없다.

## 8. Source Location

`std::source_location`을 쓴다. Logging API는 기본 인자로 받고, 매크로는 반드시
실제 호출 위치를 캡처한다.

```cpp
std::source_location location = std::source_location::current();
```

Sink가 이를 읽어 표시할지 말지는 Sink가 정한다. 현재 file sink만 표시한다.

## 9. LogSink

```cpp
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogRecord& record) noexcept = 0;
};
```

`LogManager`는 생성 시 최소 하나의 유효한 `LogSink`을 요구한다.

## 10. Multi-Sink

`LogManager`는 여러 Sink를 다룬다.

```cpp
explicit LogManager(std::span<LogSink*> sinks) noexcept;
explicit LogManager(LogSink& sink) noexcept;
```

- 레코드를 **한 번만** 만들어 모든 Sink에 넘긴다. 포맷과 필터링도 한 번만 한다.
- Sink가 N개여도 비용이 N배로 늘지 않는다.
- Sink 배열과 각 Sink의 수명은 호출자가 책임진다.
- 목록에 `nullptr`이 있으면 건너뛴다.

현재 사용되는 출력 대상은 Console과 File 두 가지다. debugger sink는 실제로
필요해질 때 추가한다.

## 11. Sink의 책임

Sink는 실제 출력 또는 저장을 담당한다. 현재 구현은 다음 두 가지다.

```text
Win32ConsoleLogSink — 표준 출력/표준 오류, 레벨별 색
Win32FileLogSink    — 실행 시각이 붙은 파일, 호출 위치 포함
```

Core 로깅 시스템은 `std::cout`, `std::cerr`를 직접 쓰지 않는다.

## 12. LogColor

`LogColor`는 `LogRecord`에 포함하지 않는다. 색상은 로그 데이터가 아니라
**출력 표현 방식**이기 때문이다.

```text
Error → Red
Warn  → Yellow
Info  → White (터미널 기본색)
Trace → Grey
Debug → Cyan
Fatal → BrightRed
```

색상 정책은 `Win32ConsoleLogSink`가 소유한다. `White`는 터미널 기본색이며
ANSI 리셋 시퀀스로 표현된다.

VT 시퀀스 처리를 켤 수 없는 환경(리다이렉트된 핸들, 구형 콘솔 호스트)에서는
색을 붙이지 않는다. 이스케이프 코드가 그대로 화면에 보이는 것을 막기 위해서다.

## 13. Fatal 처리

Fatal은 버퍼를 비우며 즉시 flush하고, 그 이후에도 시스템은 계속 동작한다.

종료하지 않는다. Fatal 처리 정책(종료 여부)은 아직 정하지 않았다.

## 14. Buffer와 Flush

File sink는 버퍼에 쌓다가 임계값에 도달하면 파일로 넘긴다.

- 버퍼 크기와 flush 임계값은 생성 시 주입받는다.
- 임계값이 0이거나 버퍼 크기를 넘으면 버퍼 크기로 보정한다.
- Fatal은 쌓인 내용과 함께 즉시 flush한다.
- 버퍼보다 긴 한 줄은 버퍼를 거치지 않고 단독으로 쓴다.
- 파일 스트림 버퍼도 함께 비운다.

```cpp
struct FileLogBufferPolicy {
    std::size_t capacity{1024};
    std::size_t flushThreshold{1024 * 80 / 100};
};
```

## 15. 로그 파일

실행 파일 옆 `logs/` 디렉터리에 만든다. 파일명에 실행 시각을 밀리초까지 넣는다.

```text
logs/nxt_20261005_153015_123.log
```

- 시각이 들어가 매 실행마다 새 파일이 생기므로 지난 로그가 남는다.
- 같은 밀리초에 두 번 실행해 파일명이 겹치면 기존 내용을 지운다.
- 열기는 trunc 모드다. 두 실행의 로그가 섞이면 어느 쪽이 문제인지 알 수 없다.

## 16. LogManager Lifetime

`LogManager`의 소유권은 호출자가 가진다. 전역 로깅 시스템이 수명을 관리하지 않는다.

다음 조건을 전제로 한다.

1. 다른 스레드가 로깅을 시작하기 전에 `setLogManager()`를 호출한다.
2. 로깅이 끝날 때까지 `LogManager`가 살아 있다.
3. 초기화 후에는 `LogManager`를 교체하지 않는다.

`setLogManager()`에 해제 API가 없다는 점은 테스트 작성 시 주의한다. 지역 변수를
등록하면 그 테스트가 끝난 뒤 남은 테스트들이 죽은 manager를 역참조한다.

## 17. Manager가 등록되지 않은 경우

초기화 이전 로그를 버린다. 프로그램이 crash하지 않는다.

## 18. Exception 정책

Logging API는 예외를 외부로 전파하지 않는다. 주요 API는 `noexcept`다.

`std::string`의 메모리 할당 실패 같은 비정상적인 메모리 부족 상황까지 정상적인
복구 대상으로 설계하지 않는다. `noexcept`의 의미는 일반적인 로깅 경로에서
exception propagation을 하지 않는다는 것이다.

## 19. Message와 Formatting

메시지는 `std::format`으로 포맷한다. 매크로는 variadic다.

```cpp
NXT_LOG_INFO(Engine, FrameLoop, "initialized in {} ms", elapsedMs);
```

형식 지정자가 잘못되면 컴파일 시점에 오류가 난다. 런타임에 실패하지 않는다.

`std::format`을 쓰지 않는 대신, 완성된 문자열을 미리 만들어 넘기는 방식은
제외한다. 로그가 값을 설명하지 못하면 디버깅에 쓸모가 없다.

## 20. Disabled Log의 처리

호출자가 이미 메시지를 생성한 뒤 `LogManager`가 filtering한다.

```cpp
NXT_LOG_DEBUG(Core, Diagnostics, createExpensiveMessage());
```

Debug가 비활성화되어 있어도 `createExpensiveMessage()`는 호출될 수 있다.
성능 문제가 측정되면 filtering을 message construction보다 앞당기는 별도
API를 고려한다. 지금은 측정 근거가 없다.

## 21. Thread Safety

`LogManager`의 상태와 Sink 호출은 모두 하나의 mutex로 보호한다.

```text
mutex lock → filter → LogRecord 생성 → sink.write()
```

따라서 Sink 구현 자체는 별도의 thread-safety를 요구받지 않는다.

Atomic ordering은 필요한 최소 수준을 쓴다.

## 22. String Conversion

```cpp
std::string_view toString(LogLayer layer) noexcept;
std::string_view toString(LogSubsystem subsystem) noexcept;
void appendCategoryGroups(std::string& out, LogCategory category);
```

`toString`은 각 축의 이름만 반환한다. 대괄호 조립은 `appendCategoryGroups`가
한다. 두 시크가 같은 형식을 쓰므로 한 곳에 두는 편이 낫다.

```text
toString(LogLayer::Platform)              →  "Platform"
toString(LogSubsystem::Win32)             →  "Win32"
toString(LogSubsystem::None)              →  ""
appendCategoryGroups(Platform/Win32)     →  "[Platform][Win32]"
appendCategoryGroups(Engine/None)        →  "[Engine]"
```

## 23. Global Logging Frontend

사용자는 `LogManager::write()`를 직접 호출하지 않고 매크로를 쓴다.

```cpp
NXT_LOG_TRACE(layer, subsystem, ...)
NXT_LOG_DEBUG(layer, subsystem, ...)
NXT_LOG_INFO(layer, subsystem, ...)
NXT_LOG_WARN(layer, subsystem, ...)
NXT_LOG_ERROR(layer, subsystem, ...)
NXT_LOG_FATAL(layer, subsystem, ...)
```

`layer`와 `subsystem` 인자는 열거값 이름만 받는다. 매크로가 `LogLayer::`와
`LogSubsystem::`을 붙이므로 접두사를 함께 쓰면 중복된다.

```cpp
NXT_LOG_INFO(Platform, Window, "...");              // 맞다
NXT_LOG_INFO(LogLayer::Platform, Window, "...");     // 중복으로 실패한다
```

하위 시스템이 없는 모듈 루트 파일은 `None`을 명시한다.

```cpp
NXT_LOG_INFO(Engine, None, "Engine initialized");
```

## 24. 구현 범위

### 구현한 것

- `LogLevel`, `LogCategory`, `LogRecord`, `LogSink`, `LogManager`
- Global LogManager registration, Global logging frontend
- Global level filtering, Category level override
- Source location, Thread ID, Timestamp
- `toString()`, `std::format` 기반 포맷
- Multi-sink fan-out
- Console sink (색 포함), File sink (버퍼링, 호출 위치)

### 아직 구현하지 않은 것

- Async Logging, Logging Thread, Lock-free Logging Queue
- File rotation, Log file management
- Debugger sink
- Fatal 종료 정책, Flush 정책의 레벨별 분리
- 성능 최적화용 logging fast path

## 25. 핵심 원칙

1. `LogManager`는 Singleton으로 만들지 않는다.
2. Core는 실제 출력 장치를 직접 알지 않는다.
3. 실제 출력은 `LogSink`가 담당한다.
4. `LogRecord`는 로그 메시지를 `std::string`으로 소유한다.
5. Category와 Level은 독립적으로 관리한다.
6. Global Level을 기본값으로 사용하고 Category별 override를 선택적으로 허용한다.
7. 색상 정보는 `LogRecord`에 넣지 않는다.
8. 현재 구현은 synchronous logging으로 유지한다.
9. 향후 asynchronous logging으로 확장할 수 있도록 `LogRecord`의 lifetime을 독립적으로 유지한다.
10. Logging API는 일반적인 경로에서 exception을 외부로 전파하지 않는다.
11. Core 로깅 시스템에 불필요한 외부 라이브러리나 OS 의존성을 추가하지 않는다.
12. 단순히 "상용 엔진에서 흔하다"는 이유만으로 기능을 추가하지 않는다.
13. 현재 확정된 API와 책임 경계를 유지하면서 확장한다.
14. public API에는 한국어 Doxygen 주석을 사용하고, 구현 세부사항에는 필요한 경우 일반적인 한국어 `//` 주석을 사용한다.