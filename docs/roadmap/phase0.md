# Phase 0 — 삼각형

> 목표: **창에 삼각형 하나를 띄운다.**
>
> 산출물은 삼각형이 아니라 **"이 아키텍처로 엔진이 간다"는 증거**다.

---

## 현재 상태

| 단계 | 상태 |
|---|---|
| **A1 환경** | 🟢 완료. SDK 1.4.363.0 설치, `find_package(Vulkan)` 동작 확인 |
| **A2 스켈레톤 배선** | 🟢 완료. Vulkan 소스 0개로 target 7개 생성, 빌드·테스트 통과 |
| **A3 창** | 🟢 완료. `Window` interface + `Win32Window` (HWND) |
| **A4 RHI 스파인** | 🟡 착수. CMake 배선·디렉터리 구조 완료, `rhi/` 6개 헤더 작성. Vulkan 소스 0줄 |
| **A5 프레임 루프** | 🔴 `engine/frame_loop.*` 0바이트 |
| **B 삼각형** | 🔴 미착수 |
| **C 계층 배선** | 🔴 미착수 |

Vulkan 런타임은 준비돼 있다 (RTX 4060, API 1.4.351).

### 이미 되어 있어 재사용되는 것

| 파일 | Phase 0에서의 용도 |
|---|---|
| `core/diagnostics/log.{hpp,cpp}` | Vulkan 디버그 메시지 출력. `LogSubsystem::Vulkan`이 이미 있음 |
| `core/diagnostics/assert.{hpp,cpp}` | `VK_CHECK`의 기반 |
| `core/time/{time,timer}.hpp` | 프레임 타이밍, CPU/GPU 분리 측정 |
| `platform/.../win32_console_log_sink.*` | 콘솔 출력 |
| `platform/.../win32_file_log_sink.*` | 파일 출력 |

### 만들어야 하는 것

| 파일 | 상태 |
|---|---|
| `core/memory/{arena,pool,allocator}.hpp` | 구현됨. **Phase 0에 소비자 없음** — Phase 1 첫 사용 |
| `core/handle/*.hpp` | 구현됨. Phase 2 애셋 핸들로 사용 |

---

## 의존 방향

```text
nxt_core → nxt_platform → nxt_graphics → nxt_renderer → nxt_engine
                                              ↓
                                    backends/vulkan  (Vulkan을 아는 곳은 여기뿐)
```

역방향 의존이 없어야 한다. 이 다이어그램의 화살표가 모두 실제 코드 경로가 되면 Phase 0 끝.

설계 근거는 [`../design/rhi.md`](../design/rhi.md),
[`../design/frame.md`](../design/frame.md),
[`../design/scene_world.md`](../design/scene_world.md).

---

# 0-A — 색으로만 채워지는 창

```text
Instance → Surface → Device → Memory → Swapchain → CommandBuffer → present
```

파이프라인·메시·셰이더를 빼고 GPU 스파인만 최소로 만든다.

## A1. 환경

- [x] Vulkan SDK 설치 — 1.4.363.0
- [x] `find_package(Vulkan REQUIRED)` 연결
- [x] include · `vulkan-1.lib` · glslang 경로 탐지 확인
- [x] GPU 초기화 1회 확인 — SDK 샘플로

## A2. 스켈레톤 배선 (C++ 코드 없음)

- [x] `src/nxt/CMakeLists.txt`에 모듈 7개 등록
- [x] `src/nxt/math/` 생성 — `nxt_math` INTERFACE 타깃
- [x] `graphics` `renderer` `engine` `assets` CMakeLists 작성
- [x] `graphics/backends/vulkan/CMakeLists.txt` 작성
- [x] `platform/CMakeLists.txt` — `.hpp`를 `add_library` 소스에서 빼고 `FILE_SET`으로
- [x] `platform/backends/windows` — `win32_window.hpp`를 공개 계약에서 제외
- [x] `platform/backends` — `if(WIN32)` 가드
- [x] `apps/sandbox` — `nxt_core` 명시적 링크
- [x] 죽은 placeholder CMakeLists 13개 삭제
- [x] `cmake/CompileShaders.cmake` 추가
- [x] **검증: Vulkan 소스 없이 target 7개 생성, 빌드 통과, 테스트 2개 통과**

생성된 타깃:

```text
nxt_core  nxt_math  nxt_platform  nxt_graphics  nxt_renderer  nxt_engine  nxt_assets  sandbox
```

A2 시점에 `nxt_graphics`는 소스가 없어 INTERFACE였다. A4에서 첫 `.cpp`가 생기며 STATIC으로 전환했다.

### CMake 규칙

```text
모듈당 CMakeLists 하나에 모든 헤더를 FILE_SET으로 등록한다.
백엔드만 예외로 backends/<os>/가 자체 CMakeLists를 가진다.
FILE_SET 이름이 다르면 TYPE HEADERS를 명시하고 소문자로 쓴다.
공개 헤더는 FILE_SET HEADERS, 비공개는 별도 이름으로 PRIVATE.
```

## A3. 창

- [x] `platform/window/window.hpp` — interface
- [x] `platform/backends/windows/win32_window.{hpp,cpp}` — HWND

```cpp
class Window {
public:
    virtual ~Window() = default;
    virtual void* nativeHandle() const noexcept = 0;  // HWND
    virtual bool isOpen() const noexcept = 0;
    virtual void pollEvents() = 0;
};
```

platform은 `HWND`까지만 준다. `VkSurfaceKHR`를 만드는 일은 graphics의 몫이다.

## A4. RHI 스파인

구조와 CMake 배선 (완료)

- [x] `graphics/runtime/` 삭제. `rhi/`로 통합한다 (아래 "착수 시 결정할 것" 참조)
- [x] `nxt_graphics` INTERFACE → STATIC
- [x] Vulkan include/link를 `INTERFACE` → `SYSTEM PRIVATE`로 전환
- [x] `nxt_core` · `nxt_platform` 링크, `/W4`, `VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1`
- [x] `FILE_SET HEADERS`(공개) + `FILE_SET backend_headers`(백엔드) 배선
- [x] **검증: `nxt_graphics.vcxproj` 생성. `renderer`·`engine`·`sandbox`에 Vulkan include 경로 없음**

공개 헤더 (`src/nxt/graphics/rhi/`)

- [ ] `rhi.hpp` — umbrella
- [x] `handle.hpp` — `OpaqueHandle`, `ResourceHandle<Tag>`
- [x] `enums.hpp` — Format, Topology, PresentMode, LoadOp, StoreOp, IndexType, BufferUsage
- [x] `memory.hpp` — GPU 메모리, `MemoryProperty`, allocate·free·map·unmap
- [ ] `instance.hpp` — Instance, Surface
- [ ] `device.hpp` — Device, Queue
- [ ] `swapchain.hpp`
- [x] `semaphore.hpp` — Semaphore (`frame.md` §5가 요구. A4 목록에 누락돼 있었다)
- [x] `fence.hpp` — create·destroy·wait·reset·isSignaled
- [ ] `command_buffer.hpp` — CommandBuffer, CommandEncoder, RenderPassDesc
- [x] `image.hpp` — ImageView (`Image` 자체는 B4)
- [ ] `resource_deletion.hpp` — fence 기반 지연 파괴

### RHI 타입은 두 패턴을 함께 쓴다

하나로 통일하지 않는다. 상태의 성격이 다르다.

```text
값 핸들 + 자유 함수       Fence  Semaphore  Memory  CommandBuffer
                         리소스 하나가 핸들 하나면 충분하다

추상 클래스 + factory     Instance  Device  Swapchain
                         vk 핸들 외에 자체 상태가 있고 해제 순서가 있다
                         Swapchain은 format·imageCount·view 배열을 들고 있다
```

```cpp
using Fence = ResourceHandle<FenceTag>;          // 값 타입
void waitFence(const Device& device, const Fence& fence);

class Swapchain {                                 // 다형성
public:
    static std::unique_ptr<Swapchain> create(const SwapchainDesc&);
    virtual ~Swapchain() = default;
    virtual ImageView imageView(std::uint32_t index) const = 0;
};
```

**`Instance`·`Device`·`Swapchain`의 `create()`는 rhi/에 선언만 하고 backend가 정의한다.**
그래서 factory를 RHI 밖에서 작성할 수 없다 — 규칙이 관례가 아니라 빌드 구조로 강제된다.

`Device`의 생성은 `Instance::createDevice()`가 맡는다. Instance가 Device를 만드는
유일한 지점이기 때문이다.

Fence를 `unique_ptr`로 만들지 않는 이유는 `frame.md` §5의 두 배열 때문이다.

```text
acquire_semaphores[framesInFlight]        ← 프레임 인덱스
submit_semaphores[swapchainImageCount]    ← 이미지 인덱스
```

`std::vector<std::unique_ptr<Semaphore>>`를 색인해야 하면 이 규칙이 성에 안 맞는다.

### Semaphore에 단독 대기 연산이 없다

Vulkan의 semaphore 대기는 항상 submit·present의 wait 목록에 넣어서 한다. 독립된
"semaphore 대기"가 존재하지 않는다. CPU가 기다리려면 fence를 쓴다.

그래서 `semaphore.hpp`는 생성과 해제만 가진다. device 전체를 기다리는 일도
fence로 해결한다 (`frame.md` §6 리사이즈 2번도 fence 대기). `rhi.md` §8이
"Pipeline barrier는 submit의 암묵적 대기로 충분하다"고 한 이유와 같다.

`waitFence`에 timeout 인자를 넣지 않은 것도 같은 이유다. `frame.md` §5가 "무한정
기다리는 쪽이 옳다고 가정한다"고 명시했다. 호출자가 없는 timeout 파라미터는
`CODING_RULES.md` §10이 금지하는 요구사항 없는 추상화다.

### GPU 객체는 디바이스를 명시적으로 받는다

`fence`·`semaphore`·`memory`의 모든 연산은 첫 인자가 `const Device&`다.

```cpp
Fence createFence(const Device& device);
Memory allocateMemory(const Device& device, std::uint64_t size, MemoryProperty required);
```

이유는 Vulkan의 규약 그대로다. GPU 객체의 생성과 소멸은 전부 디바이스가 행위한다.

```text
vkAllocateMemory(device, ...)      vkCreateFence(device, ...)
vkMapMemory(device, ...)           vkWaitForFences(device, ...)
vkFreeMemory(device, ...)           vkResetFences(device, ...)
```

디바이스를 인자로 받지 않으면 백엔드가 숨은 상태를 들고 있어야 하고, 그 순간
"누가 생성·소유·파괴하는가"를 코드만으로 알 수 없게 된다 (`CODING_RULES.md` §9).

`Instance`이 만드는 것은 `Surface`와 `Device` 둘뿐이고, 나머지는 전부 `Device`가
만든다. 이 규칙은 `instance.hpp`부터 계속 적용된다.

`class Device;` 전방선언만으로 충분하므로 `fence.hpp`·`semaphore.hpp`·`memory.hpp`은
`device.hpp`를 include하지 않는다. 세 파일은 서로에게도 의존하지 않는 리프다.

### Memory는 CPU 할당자와 대체 관계가 아니다

```text
SystemAllocator  →  _aligned_malloc / std::aligned_alloc        →  CPU 가상 메모리
rhi::Memory      →  vkAllocateMemory(memoryTypeBits) + vkMapMemory  →  GPU 메모리
```

`MemoryProperty`로 요구한 성질을 모두 가진 타입을 GPU가 찾지 못하면 할당이 실패한다.
어떤 GPU는 `HostVisible`과 `DeviceLocal`을 동시에 주지 않는다.

Phase 0의 업로드 버퍼는 아래 조합이면 충분하다. `HostCoherent` 덕분에 명시적 flush가
없다 — 그 성질이 없으면 CPU 쓰기가 GPU에 반영되지 않는다.

```cpp
MemoryProperty::HostVisible | MemoryProperty::HostCoherent   // 메모리 성질
BufferUsage::TransferDst                                      // 버퍼 용도
```

`HostVisible`은 PCIe를 지나 CPU가 접근할 수 있게 하고, `HostCoherent`은 그 쓰기를
별도 신호 없이 GPU가 보게 만든다. 할당은 한두 번뿐이라 allocator 계층은 만들지
않는다. VMA는 Phase 2에서 검토한다.

### 핸들은 `nxt_core::handle::Handle`를 상속하지 않는다

| | `core::handle::Handle<Tag>` | `rhi::ResourceHandle<Tag>` |
|---|---|---|
| 저장 | `{index: u32, generation: u32}` | `value: u64` |
| 짝 | `HandleManager` 필수 | 없음 |
| 세대 관리 | 직접 함 | Vulkan이 함 |
| 쓰임새 | Phase 2 애셋 (`scene_world.md` §7) | Phase 0 GPU 리소스 |

Vulkan은 `VkImageView`와 `VkBuffer`를 다른 타입으로 선언하지만 실제로는 둘 다 64비트 값이다.
그 값들을 그대로 다루면 실수를 컴파일 시점에 못 잡는다. 태그로 종류를 구분해 막는다.

### `ImageView`만 A4에 먼저 넣는 이유

B4가 깊이 버퍼를 앞당긴 것과 같은 논리다.

> **깊이 버퍼를 여기서 넣는다.** 삼각형 하나에는 시각적으로 필요 없지만, 나중에 넣으면
> 다섯 곳을 다시 건드린다.

스왑체인 이미지는 0-A부터 render pass의 color attachment 자리에 view가 들어가야 한다.
`image.hpp`를 A4에 만들고 `ImageView`만 넣은 뒤, B4에서 `Image`를 더한다.

### 핸들 헤더는 어떤 TU에서도 include되지 않는다

`FILE_SET HEADERS`는 IDE에 파일로 노출만 하고 **컴파일하지 않는다.** `.cpp` 하나가
헤더를 include하기 전까지는 그 안에 문법 오류가 있어도 빌드가 통과한다.

`A4` 검증은 임시 TU로 헤더를 단독 컴파일해서 한다. `/W4 /WX`로 올린다.

```powershell
cl /std:c++20 /W4 /WX /utf-8 /permissive- /c /I "%SRC%" rhi_check.cpp
```

헤더는 자기 완결이어야 한다 (`CODING_RULES.md` §5). 필요한 헤더는 직접 include하고
transitive include에 기대지 않는다.

시그니처까지 고정하려면 함수 포인터에 대입한다. `const&`가 빠졌다고 컴파일은 된다.

```cpp
static_assert(std::is_same_v<decltype(&waitFence), void (*)(const Device&, const Fence&)>);
```

**검증 코드의 기대값은 헤더에서 복사하지 않는다.** 비트 값을 직접 세어 고정한다.

```cpp
constexpr BufferUsage kVertices = BufferUsage::Vertex | BufferUsage::TransferDst;
static_assert(static_cast<std::uint32_t>(kVertices) == 33u);   // Vertex=1<<0, TransferDst=1<<5
```

`TransferDst`는 `1u << 5`라서 32다. `1u << 4`(TransferSrc)와 헷갈리기 쉽고,
기대값을 잘못 쓰면 컴파일 자체가 아니라 검증이 조용히 틀어진다.

MSVC 콘솔에서 한국어 오류 메시지가 깨지면 오류 코드를 함께 본다. `static_assert`가
실패한 줄과 다른 줄에서 그 진짜 원인이 보이는 경우가 있다.

백엔드 구현 (`src/nxt/graphics/backends/vulkan/`)

- [x] `vulkan_types.hpp` — vk:: 타입이 존재하는 유일한 파일
- [x] `vulkan_check.hpp` — `VK_CHECK`
- [ ] `vulkan_instance.cpp` · `vulkan_device.cpp` · `vulkan_memory.cpp`
- [ ] `vulkan_swapchain.cpp` · `vulkan_command_buffer.cpp` · `vulkan_fence.cpp` · `vulkan_semaphore.cpp`
- [ ] `vulkan_resource_deletion.cpp`

`vulkan_loader`를 직접 작성하지 않는다. `find_package(Vulkan)`이 준 `Vulkan::Vulkan`을
링크하고 Vulkan-Hpp의 동적 디스패처를 쓴다. 근거는
[`../design/rhi.md` §6](../design/rhi.md) — 이전 문서의 "직접 로드"는 철회했다.

**`vulkan_types.hpp`가 enums를 소비하는 지점이다.** `rhi::Format` ↔ `VkFormat` 매핑을
여기에 둔다. 배열 조회 대신 `switch`를 쓴다 — `/W4`가 `case` 누락을 잡아주므로,
나중에 열거형을 늘렸는데 매핑을 빠뜨리면 컴파일 시점에 드러난다.

**RHI 밖으로 `vk::`가 새지 않는지 그 자체로 검증되는 지점이다.** 이 파일이
`vulkan/vulkan.hpp`를 처음 include하고 `ResourceHandle<Tag>`를 처음 역변환한다.

`vulkan_types.hpp`만 `vulkan/vulkan.hpp`를 include한다. 다른 어떤 파일도 직접 하지 않는다.

**dispatchable 핸들(Instance · Device · Queue)은 `fromVkHandle`로 옮길 수 없다.** 값이
포인터라 `static_cast`가 성립하지 않기 때문이다. `reinterpret_cast`를 쓰는 변환을
instance/device 단계에서 `vulkan_types.hpp`에 추가한다.

**`VK_CHECK`는 `vk::Result`만 받는다.** 값이 있는 호출은 `ResultValue<T>`를 돌려주므로
먼저 받아 `.result`를 넘기고 `.value`로 값을 꺼낸다. 담긴 값을 버리지 않게 하려는
의도다 (`VULKAN_HPP_NO_EXCEPTIONS`의 enhanced 함수 반환 형태).

```cpp
const auto created = vk::createInstance(createInfo);
VK_CHECK(created.result);
vk::Instance instance = created.value;
```

**`memory.hpp`가 별도 계층인 이유.** `SystemAllocator`로 대체되지 않는다.

```text
SystemAllocator  →  _aligned_malloc / std::aligned_alloc        →  CPU 가상 메모리
rhi::Memory      →  vkAllocateMemory(memoryTypeBits) + vkMapMemory  →  GPU 메모리
```

memory type bits(`DEVICE_LOCAL` `HOST_VISIBLE` `HOST_COHERENT` `HOST_CACHED`)
와 mapping을 다뤄야 한다. `SystemAllocator`를 감싸도 둘 다 얻을 수 없다. **대체 관계가 아니다.**

필요한 버퍼가 1~2개이므로 allocator abstraction은 만들지 않는다. 할당이 많아지는 Phase 2에서 VMA를 검토한다.

## A5. 프레임 루프

- [ ] `engine/frame_loop.{hpp,cpp}` — acquire → clear → submit → present

설계 고정 사항 ([`frame.md`](../design/frame.md))

```text
minImageCount = 3        트리플 버퍼링. 더블은 VSync를 놓치면 30fps로 떨어진다
present mode  = FIFO     MAILBOX는 입력 지연이 필요할 때만
present semaphore의 색인은 프레임 인덱스가 아니라 이미지 인덱스
destroy()는 지연 목록에 넣고, 그 리소스를 마지막으로 쓴 프레임의 fence가
  신호된 뒤에 실제 파괴한다
파괴 순서는 Pipeline → Buffer/Image
```

## A6. 0-A 검증

- [ ] 창이 열린다
- [ ] 지정한 색으로 화면이 채워진다
- [ ] 리사이즈해도 깨지지 않는다
- [ ] 캡처 도구로 60fps 이상 확인된다
- [ ] 리소스 파괴 후 GPU 크래시가 없다 (검증 레이어 ON 기준)

---

# 0-B — 삼각형

```text
+ Shader → Pipeline → VertexBuffer → draw + 깊이 버퍼
```

## B1. 수학

- [ ] `src/nxt/math/{vec3,mat4}.hpp` — `nxt_math` target, namespace `nxt::math`

## B2. 셰이더

- [ ] `shaders/triangle.vert` · `triangle.frag`
- [x] CMake 셰이더 컴파일 규칙 — `cmake/CompileShaders.cmake`
- [ ] `nxt_add_shader`로 SPIR-V 등록
- [ ] `graphics/rhi/shader.hpp` + `vulkan_shader.cpp`

## B3. 파이프라인

- [ ] `graphics/rhi/pipeline.hpp` + `vulkan_pipeline.cpp`
  — GraphicsPipeline, VertexLayout, rasterizer 상태

## B4. 버퍼와 이미지

- [ ] `graphics/rhi/buffer.hpp` + `vulkan_buffer.cpp`
- [ ] `graphics/rhi/image.hpp` + `vulkan_image.cpp`

**깊이 버퍼를 여기서 넣는다.** 삼각형 하나에는 시각적으로 필요 없지만, 나중에 넣으면
다섯 곳을 다시 건드린다.

```text
render pass layout · framebuffer · 파이프라인 상태(compare op) · clear 값 · render target 기술자
```

depth는 어차피 Phase 3에 존재한다. 그때 넣으면 Phase 0의 렌더 패스를 통째로 다시 만들어야 한다.

## B5. 렌더러

- [ ] `renderer/mesh/mesh.hpp` — CPU 정점
- [ ] `renderer/passes/triangle_pass.{hpp,cpp}`

## B6. 0-B 검증

- [ ] 삼각형이 보인다
- [ ] 깊이 테스트가 켜져 있고 뒤쪽 삼각형이 가려진다
- [ ] `grep -rn "vk::" src/nxt/graphics/rhi/` → **0건**
- [ ] `grep -rn "backends/vulkan" src/nxt/renderer/ src/nxt/engine/` → **0건**

두 grep이 실패하면 RHI 경계가 이미 붕괴한 것이다. 여기서 고치지 않으면 이후 수정 비용이 배가된다.

---

# 0-C — 계층 배선

```text
+ Engine가 frame loop을 소유하고 Renderer가 mesh N개를 그린다
```

## C1. 배선

- [ ] `renderer/frame_context.{hpp,cpp}` — 매 프레임 임시 상태
- [ ] `renderer/renderer.{hpp,cpp}`
- [ ] `engine/engine.{hpp,cpp}` — 서브시스템 조립과 소유
- [ ] `apps/sandbox/main.cpp` — 로깅 스모크 테스트를 삼각형 데모로 교체

## C2. 0-C 검증

- [ ] mesh 개수를 바꾸면 그 개수만큼 그려진다
- [ ] 창을 닫으면 깨끗하게 종료된다
- [ ] 리사이즈 후에도 계속 그려진다

---

## 제외

- CPU 런타임 (`jobs` `event` `concurrency`) → [Phase 1](phase1.md)
- Asset 로딩 → [Phase 2](phase2.md)
- Render graph · multi-pass · shadow → Phase 3
- Descriptor set · bindless · async compute → Phase 4 이후
- Material / lighting → Phase 3
- SIMD · Vulkan 파이프라인 고도화 → 측정 없이 도입하지 않는다
- Linux backend → Vulkan 로더로 교체 가능한지 확인되면 그때

## 착수 시 결정할 것

| 질문 | 기본값 | 상태 |
|---|---|---|
| `graphics/runtime/` 생존 여부 | 삭제하고 `rhi/`로 통합 | **결정됨 — 삭제.** `frame.md` §3/§7의 `graphics/runtime`은 디렉터리가 아니라 모듈 `nxt_graphics`를 가리키는 표현이다 |
| namespace 규칙 | flat한 디렉터리는 이름 반복 안 함 (`nxt::math`) | **결정됨 — `nxt::graphics::rhi`.** `rhi.md` §8이 이미 `nxt_graphics::rhi::Memory`로 이름을 확정했다. 백엔드는 `nxt::graphics::vulkan` (`platform::win32` 선례) |
| graphics 테스트 | Phase 0는 없음 | **결정됨 — 없음.** 검증은 grep 게이트 + 단독 컴파일로 한다. null 백엔드는 요구되지 않았고 그만큼 비싸다 |
| instance 확장 | `VK_EXT_debug_utils`만 | **결정됨.** 검증 레이어는 A6에서 붙인다 |
| `render pass` 이름 충돌 | RHI는 기술자(descriptor) | **결정됨 — `rhi::RenderPassDesc`.** renderer 쪽 `RenderPass`(개별 패스)와 구분한다 |

### `frame.md`의 `graphics/runtime` 표기

`frame.md` §3과 §7이 `graphics/runtime`을 디렉터리처럼 적었지만, 그 책임(디바이스
초기화 · 프레임 제출 관리 · present 소유권)은 `rhi.md` §8이 `rhi/`의
Instance·Device·Swapchain·CommandBuffer·Fence·ResourceDeletion에 이미 배정돼 있다.
**같은 책임을 부르는 이름이 두 개였다.** `rhi/` 쪽이 이겼으므로 코드는 `rhi/`를 따른다.