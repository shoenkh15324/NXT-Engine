# RHI 설계 결정사항

`nxt_graphics`의 경계와 추상화 높이를 정한다.

코드와의 불일치가 있다면 문서를 고치는 것을 우선한다.

---

## 1. RHI가 하는 일

RHI(Render Hardware Interface)는 GPU를 다루는 최소 인터페이스다.
**장치를 다루되, 무엇을 그릴지는 알지 않는다.**

```text
nxt_renderer  ──→  nxt_graphics / rhi  ──→  backends/vulkan  ──→  Vulkan  ──→  드라이버
   무엇을 그릴지         어떻게 그릴지           어떻게 명령할지
```

| RHI가 안다 | RHI가 모른다 |
|---|---|
| device, queue, swapchain | 씬, 씬 그래프, 컬링 |
| command buffer | 머티리얼, 셰이더의 의미 |
| pipeline, descriptor 상태 | 렌더 패스 사이의 의존 관계 |
| buffer, image, sampler | 게임 상태, 에셋, 파일 |
| fence, 동기화 | 프레임을 부르는 시점 |

`renderer`가 `VkImage` 대신 `Texture`를, `vkCmdDraw` 대신 `draw()`를 아는 것이
경계가 살아 있다는 뜻이다.

## 2. 왜 추상화해야 하는가

첫 백엔드가 Vulkan 하나뿐이어도 추상화한다. 이유는 다음 두 가지다.

1. **오류 복원 가능 영역을 늘린다.** 디버거 레이어, 캡처 도구, 검증 레이어가 GPU 경로에
   걸려 들어온다. Vulkan 타입이 상위 레벨까지 번지면 이 도구들이 전부 계층을 무시한다.
2. **renderer를 테스트할 수 있어야 한다.** Vulkan 없이 pipeline 비교나 컬링 결과를
   검증할 수 없다.

"나중에 다른 API를 추가할지도 모른다"는 이유로 하는 추상화가 아니다
(`CODING_RULES.md` §11). **지금은 Vulkan이라서** 하고, 그 대가로 얻는 위 두 가지다.

## 3. 추상화 높이 — thin RHI

**RHI는 Vulkan 명령을 감싼 얇은 껍대다.** 중간 표현단(IR)을 두지 않는다.

```cpp
// 이런 식으로 호출한다.
encoder.beginRenderPass(target);
encoder.setPipeline(pipeline);
encoder.setVertexBuffer(0, buffer);
encoder.draw(vertexCount, 1, 0, 0);
encoder.endRenderPass();
```

IR(커맨드 리스트)을 두지 않는 이유:

- IR이 있으면 백엔드마다 IR → 네이티브 변환기가 필요하다. Vulkan 하나뿐인 지금은
  그 변환기가 부적합한 중계 계층이다.
- IR을 나중에 넣으면 그때 전체 커맨드 경로를 다시 쓰게 된다. 지금 미리 넣으면
  **삼각형에 필요 없는 것**을 만드는 셈이다.

단, 이 결정은 되돌릴 수 있다. 되돌리는 시점은 "descriptor set 관리와 async compute가
Phase 0 단일 큐로 불가능해지는 시점"이다. 그때 측정하고 결정한다.

## 4. Vulkan 타입 유출 금지

**`vk::`가 등장할 수 있는 파일은 `src/nxt/graphics/backends/vulkan/` 아래뿐이다.**

```text
rhi/swapchain.hpp           VkSwapchainKHR를 쓰지 않는다
rhi/pipeline.hpp            VkPipeline를 쓰지 않는다
backends/vulkan/vulkan_types.hpp    vk:: 타입이 존재하는 유일한 파일
```

`vulkan_types.hpp`는 backend 구현부만 include한다. 헤더이므로 include 누락을 막으려면
다음 검증이 필요하다.

```bash
# Phase 0 완료 판정 항목
grep -rn "vulkan" src/nxt/graphics/rhi/
grep -rn "backends/vulkan" src/nxt/renderer/ src/nxt/engine/
```

**이 규칙이 깨진 채로 진행하면 RHI 경계가 이미 붕괴한 상태다.** 이후 수정 비용은
지금보다 훨씬 크다. 마일스톤 0-B의 완료 기준에 포함되어 있다.

RHI 내부에서는 backend-private 타입(핸들)을 쓰지 않는다. 필요하다면
`OpaqueHandle`에 값을 숨기고, 해석은 backend 구현부에만 둔다.

## 5. 모듈 구조

```text
src/nxt/graphics/
    rhi/                 공개 API. Vk를 모른다
    backends/
        vulkan/          Vulkan 구현. Vk를 안다
```

- `rhi/*.hpp`는 헤더 전용으로 충분하다. 상태는 backend 구현체가 소유한다.
- `backends/vulkan/`이 `rhi/*.hpp`를 include하는 것은 허용된다. 반대는 금지한다.
- Vulkan 헤더(`vulkan/vulkan.hpp`)는 `backends/vulkan/`만 include한다.
  전역 include 경로에 Vulkan을 노출하지 않는다.

## 6. Vulkan 로딩

Vulkan SDK를 `find_package(Vulkan)`로 찾고 `Vulkan::Vulkan`으로 링크한다.

```cmake
find_package(Vulkan REQUIRED)
target_link_libraries(nxt_graphics PUBLIC Vulkan::Vulkan)
```

`find_package`가 include 경로, 로더 라이브러리(`vulkan-1.lib`), glslang 경로를 모두 준다.
Vulkan 런타임 자체는 OS가 `vulkan-1.dll`을 제공한다.

**Vulkan-Hpp의 동적 디스패처를 쓴다.** 값을 하드코딩하지 않고 런타임에 물어보는
함수 호출은 Vulkan-Hpp가 이미 처리한다. 이를 위해 `Vulkan::Vulkan` 링크와 함께
`VULKAN_HPP_DEFAULT_DISPATCHER`를 초기화한다. 라이브러리가 바뀔 때 이를 다시
맞출 이유가 없다.

이전 문서에서 "직접 `LoadLibrary`로 `vulkan-1.dll`을 로드한다"고 했으나 철회한다.
논거는 "Vulkan 런타임이 없는 시스템에서 프로그램이 시작되지 않는다"였는데,
Windows 10+에서는 `vulkan-1.dll`이 system32에 있고 GPU 드라이버와 함께 설치된다.
**고려할 시나리오가 아니다.** 로더를 직접 작성하면 같은 일을 두 번 하게 된다.

단, include 경로는 `SYSTEM`으로 지정해야 한다. `Vulkan::Vulkan`은 include 디렉터리를
SYSTEM으로 표시하지 않아서, 그대로 쓰면 Vulkan 헤더의 경고가 `/W4` 빌드를 오염시킨다.

```cmake
target_include_directories(nxt_graphics SYSTEM PUBLIC "${Vulkan_INCLUDE_DIR}")
```

## 7. 에러 처리

```cpp
VK_CHECK(vkCreateInstance(...));
```

- 모든 Vulkan 호출 결과를 검사한다.
- 실패 시 파일명 / 줄 번호 / 호출명을 남기고 종료한다.
- GPU 오류는 대부분 복구 불가능하므로 예외를 던지지 않고 종료로 처리한다
  (`CODING_RULES.md` §13).
- 검증 레이어 메시지는 기존 `LogSink`로 흘려보낸다. 별도 출력 경로를 만들지 않는다.

검증 레이어 메시지를 `nxt_core::log`로 보내려면 `backends/vulkan/`이 core 로그를
사용해야 한다. 이것이 허용되는 유일한 core 의존 방향이다.

## 8. Phase 0에서 만드는 API

```text
Instance       생성, 지원 extension 확인, VkSurfaceKHR 생성
Device         물리 디바이스 선택, 논리 디바이스 생성, Queue 획득
Memory         vkAllocateMemory, memory type 선택, vkMapMemory  ★ core로 대체 불가
Swapchain      생성(minImageCount=3, FIFO), acquire, present, recreate
CommandBuffer  begin/end, submit
CommandEncoder render pass · pipeline · vertex/index buffer · draw
Pipeline       그래픽 파이프라인, vertex layout, rasterizer 상태
Buffer         GPU 버퍼, host-visible 매핑
Image          2D 이미지, 깊이 버퍼
Shader         SPIR-V 모듈
Fence          신호 대기
ResourceDeletion  fence 기반 지연 파괴 목록
```

### Memory가 별도 계층인 이유

`nxt_core::memory::SystemAllocator`는 `_aligned_malloc` / `std::aligned_alloc` 기반이라
**CPU 가상 메모리만** 만든다. GPU 버퍼에 쓸 수 없다.

```text
SystemAllocator  →  CPU 가상 메모리
Memory (여기)    →  vkAllocateMemory(memoryTypeBits) + vkMapMemory
```

memory type bits(`DEVICE_LOCAL` · `HOST_VISIBLE` · `HOST_COHERENT` ·
`TRANSFER_SRC` · `TRANSFER_DST`)와 mapping을 다뤄야 하며, `SystemAllocator`를
감싸도 이 둘을 얻을 수 없다. **두 계층은 대체 관계가 아니다.**

### Phase 0에서 만들지 않는 것

다음은 **지금 필요하지 않다.** 만드는 시점이 오면 그때 만든다.

- descriptor set 관리 (uniform, sampler, bindless)
- sampler
- command pool의 다중 스레드 소유권
- 다중 queue family, async compute
- dynamic rendering / 렌더 패스 2세대 API
- memory allocator(VMA 등) — Phase 0은 직접 구현, 할당이 많아지는 Phase 2에서 검토
- pipeline cache — 의존성 순서 파괴를 위해 Phase 0에서는 파이프라인을 하나만 만든다

이유는 하나다. 삼각형은 이 중 어느 것도 쓰지 않는다.
`CODING_RULES.md` §20에 따라 지금 만들면 삼각형을 그리는 데 필요 없는 추상화가 된다.

## 9. 리소스 수명 — fence 기반 지연 파괴

GPU 리소스는 즉시 파괴하지 않는다. 절차와 근거는 [`frame.md` §9](frame.md)에 있다.

```text
destroy() → 마지막 사용 프레임 기록 → 지연 목록
          → 해당 fence 신호 후 실제 vkDestroy*
```

트리플 버퍼링에서 in-flight 프레임이 여러 개이므로 즉시 파괴는 use-after-free다.
여기서는 **API만 정의한다.** 언제 파괴를 처리하고 어떤 순서로 정리하는지는
프레임 수명의 소유자인 `nxt_graphics`가 정한다.

의존성 순서도 지켜야 한다. **Pipeline → Buffer/Image** 순으로 파괴한다.
파이프라인이 버퍼를 참조하므로 역순은 죽은 핸들을 남긴다.

## 10. 렌더 패스

Phase 0은 단일 렌더 패스다. 다중 패스는 Phase 3에서 render graph와 함께 들어간다.

RHI는 render pass를 **기술자(descriptor)**로 받는다. 무엇을 그리는지는 모른다.
`nxt_renderer`가 기술자를 만들고, `nxt_graphics`는 그것을 Vulkan으로 번역한다.

이 구조 덕분에 Phase 3에서 render graph를 추가해도 RHI는 바뀌지 않는다.

## 11. 핵심 원칙

1. RHI는 GPU를 다루되 무엇을 그릴지는 알지 않는다.
2. `vk::`는 `backends/vulkan/` 밖에서 나타나지 않는다.
3. Vulkan 헤더는 전역 include 경로에 노출하지 않는다.
4. Vulkan은 `find_package(Vulkan)` + `Vulkan::Vulkan`으로 연결한다. 로드를 직접 작성하지 않는다.
5. Vulkan 헤더의 include 경로는 `SYSTEM`으로 지정한다.
6. 추상화는 지금 필요한 것만 한다. IR은 필요해질 때 넣는다.
7. 모든 GPU 오류는 검증 레이어를 포함해 로깅으로 보고한다.
8. GPU 메모리는 `nxt_graphics::rhi::Memory`가 만든다. `SystemAllocator`는 CPU 전용이다.
9. GPU 리소스는 fence 대기 없이 파괴하지 않는다.
10. 한 백엔드라도 경계는 유지한다. 지금의 대가로 얻는 것은 검증 가능성과 도구 호환성이다.