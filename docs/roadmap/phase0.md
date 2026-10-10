# Phase 0 — 삼각형

> 목표: **창에 삼각형 하나를 띄운다.**
>
> 산출물은 삼각형이 아니라 **"이 아키텍처로 엔진이 간다"는 증거**다.

---

## 현재 상태

| 단계 | 상태 |
|---|---|
| **A1 환경** | 🔴 **착수 블로커.** Vulkan SDK 없음 — 헤더도 셰이더 컴파일러도 없다 |
| **A2 스켈레톤 배선** | 🔴 미착수. 4개 모듈 CMakeLists가 0바이트 |
| **A3 창** | 🔴 `win32_window.*` 0바이트, `window.hpp`는 주석만 |
| **A4 RHI 스파인** | 🔴 전부 없음. `rhi/rhi.hpp` 63바이트 주석 |
| **A5 프레임 루프** | 🔴 `engine/frame_loop.*` 0바이트 |
| **B 삼각형** | 🔴 미착수 |
| **C 계층 배선** | 🔴 미착수 |

Vulkan 런타임은 준비돼 있다 (RTX 4060, API 1.4.351). SDK만 없다.

### 이미 되어 있어 재사용되는 것

| 파일 | Phase 0에서의 용도 |
|---|---|
| `core/diagnostics/log.{hpp,cpp}` | Vulkan 디버그 메시지 출력. `LogCategory::Graphics`가 이미 있음 |
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

- [ ] Vulkan SDK 설치 (glslang 포함)
- [ ] `find_package(Vulkan REQUIRED)` 연결
- [ ] SDK 샘플로 GPU 초기화 1회 확인

## A2. 스켈레톤 배선 (C++ 코드 없음)

- [ ] `src/nxt/CMakeLists.txt`에 `math` `graphics` `renderer` `engine` `assets` 등록
- [ ] `graphics/` `renderer/` `engine/` `assets/` CMakeLists 작성 — **지금 0바이트이라 걸어도 target이 안 생긴다**
- [ ] `platform/CMakeLists.txt:4-6` — `.hpp`를 `add_library` 소스에서 빼고 `FILE_SET HEADERS`로
- [ ] `platform/backends/windows/CMakeLists.txt` — `win32_window.hpp`를 `PUBLIC`에서 빼라
- [ ] `apps/sandbox/CMakeLists.txt` — `nxt_core` 명시적 링크 (현재 transitive에 의존)
- [ ] **검증: Vulkan 코드 없이 빌드가 통과한다**

## A3. 창

- [ ] `platform/window/window.hpp` — interface
- [ ] `platform/backends/windows/win32_window.{hpp,cpp}` — HWND

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

공개 헤더 (`src/nxt/graphics/rhi/`)

- [ ] `rhi.hpp` — umbrella
- [ ] `enums.hpp` — Format, Topology, PresentMode, LoadOp, StoreOp, IndexType, BufferUsage
- [ ] `memory.hpp` — GPU 메모리
- [ ] `instance.hpp` — Instance, Surface
- [ ] `device.hpp` — Device, Queue
- [ ] `swapchain.hpp`
- [ ] `command_buffer.hpp` — CommandBuffer, CommandEncoder
- [ ] `fence.hpp`
- [ ] `resource_deletion.hpp` — fence 기반 지연 파괴

백엔드 구현 (`src/nxt/graphics/backends/vulkan/`)

- [ ] `vulkan_types.hpp` — vk:: 타입이 존재하는 유일한 파일
- [ ] `vulkan_loader.{hpp,cpp}` — `vulkan-1.dll` 동적 로드
- [ ] `vulkan_check.hpp` — `VK_CHECK`
- [ ] `vulkan_memory.cpp` · `vulkan_instance.cpp` · `vulkan_device.cpp`
- [ ] `vulkan_swapchain.cpp` · `vulkan_command_buffer.cpp` · `vulkan_fence.cpp`
- [ ] `vulkan_resource_deletion.cpp`

**`memory.hpp`가 별도 계층인 이유.** `SystemAllocator`로 대체되지 않는다.

```text
SystemAllocator  →  _aligned_malloc / std::aligned_alloc        →  CPU 가상 메모리
rhi::Memory      →  vkAllocateMemory(memoryTypeBits) + vkMapMemory  →  GPU 메모리
```

memory type bits(`DEVICE_LOCAL` `HOST_VISIBLE` `HOST_COHERENT` `TRANSFER_SRC` `TRANSFER_DST`)
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
- [ ] CMake 셰이더 컴파일 규칙 (GLSL → SPIR-V)
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

| 질문 | 기본값 |
|---|---|
| `graphics/runtime/` 생존 여부 | 삭제하고 `rhi/`로 통합 |
| namespace 규칙 | flat한 디렉터리는 이름 반복 안 함 |
| graphics 테스트 | Phase 0는 없음 |
| instance 확장 | `VK_EXT_debug_utils`만 |