# dzn이 D32_SFLOAT 깊이 clear에서 디바이스를 잃는 문제

> 발견일: 2026-11-27
>
> 상태: 엔진은 깊이 형식을 `D32_SFLOAT_S8_UINT`로 바꿔 회피했다. 드라이버 수정 여부는
> 확인하지 않았다.
>
> 영향 범위: WSLg의 기본 Vulkan ICD `dzn`(Mesa Dozen, Vulkan-on-D3D12).
> Windows의 MSVC 빌드와 lavapipe에는 해당하지 않는다.
>
> 관련 커밋: `073a44f fix(graphics): clear depth with a format dzn can handle`

## 1. 증상

WSLg에서 Sandbox를 실행하면 창이 뜬 뒤 화면이 갱신되지 않다가 약 4초 뒤 종료된다.
로그에는 다음 순서로 남는다.

```text
[13:48:19] [INFO][Graphics][Vulkan] physical device selected: Microsoft Direct3D12 (Intel(R) Arc(TM) Graphics)
[13:48:19] [INFO][Graphics][Vulkan] swapchain created: images=4 extent=1280x720 format=B8G8R8A8Srgb present=Fifo
[13:48:19] [INFO][Renderer][Passes] record: objects=100000 visible=65025
          D3D12: Removing Device.
[13:48:23] [WARN][Graphics][Vulkan] D3D12 device removed (VK_ERROR_DEVICE_LOST)
[13:48:23] [INFO][Engine][FrameLoop] warmup: 60 frames left

========== NXT ASSERTION FAILED ==========
Expression : vulkan::toVulkanDevice(device).device().waitForFences(1, fences, vk::True, kWaitForever)
File       : src/nxt/graphics/backends/vulkan/frame/vulkan_fence.cpp
Line       : 44
Function   : waitFence
Message    : ErrorDeviceLost
===========================================
```

- 프레임이 한 장도 완료되지 않는다(`warmup: 60 frames left`).
- 시작 후 약 4초에 항상 같은 지점에서 발생한다.
- `NXT ASSERTION FAILED`는 로직 검사 실패가 아니라 GPU 오류를 치명적으로 처리하는
  엔진 경로다 (`rhi.md` §7). `VK_CHECK` → `reportVulkanFailure` → `reportAssertFailure`.

## 2. 환경

| 항목 | 값 |
|---|---|
| OS | WSL2 + WSLg |
| Vulkan 헤더·런타임 | 1.3.275 |
| ICD | `dzn` (D3D12, "not a conformant Vulkan implementation") |
| GPU 장치 | Microsoft Direct3D12 (Intel Arc) — WSLg GPU 가상화 |
| 빌드 | GCC 13.3.0, CMake 3.28.3, Ninja, Debug(validation ON) |
| 화면 | Wayland, xdg-shell, 1280x720 |

## 3. 재현

```bash
./build/gcc/apps/sandbox/sandbox --objects 1000 --log-level info
```

부하와 무관하다. `--objects 1`, `500`, `1000`, `100000` 모두 같은 시점에 실패한다.
validation layer를 꺼도(`VK_LOADER_LAYERS_DISABLE=VK_LAYER_KHRONOS_validation`) 동일하다.

## 4. 원인 분석

### 4.1 GPU 제출인가, present인가

`present()` 직전에 `device.waitIdle()`을 넣어 두 지점을 분리했다.

```text
[13:58:44] debug: waitIdle before present (frame=0)     ← 반환되지 않음
[13:58:47] D3D12 device removed (VK_ERROR_DEVICE_LOST)  ← 약 3초 뒤
```

`vkQueuePresentKHR`가 아니라 **제출한 GPU 작업이 끝나지 않는다**. gdb로 보면 애플리케이션은
`present` 안에서 대기 중인데, 이는 제출이 끝나지 않아 WSI가 기다리는 상태다. 그 사이
디바이스가 제거되고, 다음 프레임의 `waitFence`가 상실을 보고한다.

```text
#0 poll(timeout=-1)
#1 wl_display_dispatch_queue
#2..#6 libvulkan_dzn.so
#7,#8 libVkLayer_khronos_validation.so
#9 VulkanSwapchain::present()
#10 FrameLoop::frame()
```

### 4.2 커맨드 스트림 이분

| 실험 | 결과 |
|---|---|
| 빈 커맨드 버퍼 (record 생략) | 정상 |
| 색 부착물만 clear (깊이 제거) | 정상 |
| `vkCmdWriteTimestamp` 제거 | 디바이스 로스트 (무관) |
| 깊이 `D32_SFLOAT` + `loadOp = DontCare` | 정상 |
| 깊이 `D32_SFLOAT` + `loadOp = Clear` | **디바이스 로스트** |
| 깊이 `D32_SFLOAT` + `Clear` + `storeOp = Store` | **디바이스 로스트** |
| 깊이 `D24_UNORM_S8_UINT` + `Clear` | 정상 |
| 깊이 `D32_SFLOAT_S8_UINT` + `Clear` | 정상 |

즉 **깊이 전용 32비트 float 형식의 render pass clear**만 실패한다. 깊이 부착물이 있어도
clear하지 않으면(`DontCare`) 정상이고, clear를 해도 스텐실이 붙은 형식이면 정상이다.

### 4.3 배제한 것

| 의심 | 근거 |
|---|---|
| 드로우 콜 수 | 1개에서도 동일하게 실패 |
| present/WSI | 빈 커맨드 버퍼는 present까지 정상 |
| validation layer | 레이어를 꺼도 동일하게 실패 |
| timestamp query | 제거해도 동일하게 실패 |
| 깊이 clear 외 렌더 명령 | 깊이 없는 pass, 깊이 `DontCare` pass 모두 정상 |
| 엔진 코드의 API 오용 | validation VUID/ERROR 없음 |
| 드라이버가 Wayland WSI 자체를 못 함 | 같은 dzn에서 `vkcube-wayland`가 58fps로 정상 표시 |
| 엔진/플랫폼 공통 경로 | 같은 바이너리가 lavapipe에서 59fps로 정상 |

`VK_FORMAT_D32_SFLOAT` 깊이 부착물의 clear는 Vulkan 명세상 정상 사용이다. 따라서 이는
dzn의 D3D12 변환 경로 문제로 판단한다.

## 5. 회피

깊이 형식을 `D32_SFLOAT`에서 `D32_SFLOAT_S8_UINT`로 바꿨다.

```cpp
// src/nxt/engine/frame_loop.hpp
static constexpr rhi::Format kDepthFormat = rhi::Format::D32SFloatS8Uint;
```

- 32비트 깊이 정밀도는 그대로다. 스텐실 8비트는 아직 쓰지 않는다.
- 부착물 크기는 픽셀당 8바이트가 되어 1280x720에서 약 7.4MB다(이전 3.7MB).
- `D24_UNORM_S8_UINT`로도 회피되지만 깊이 정밀도가 24비트로 줄어 선택하지 않았다.
- 렌더 패스·파이프라인·깊이 이미지가 모두 `FrameLoop::kDepthFormat` 하나를 쓰므로
  변경 지점은 한 곳이다.

## 6. 검증

Debug + validation 환경에서 확인했다. 임시 진단 스위치는 모두 제거한 상태다.

| 실행 | 결과 |
|---|---|
| dzn, 1000 오브젝트 | 약 59~61fps, `gpu p50 0.12ms`, `draws=676`, 디바이스 로스트 없음 |
| dzn, 100000 오브젝트 | `draws=65025`, 초당 9프레임(CPU 110ms, push-constant draw 65k), 로스트 없음 |
| lavapipe, 1000 오브젝트 | 약 59fps, 동일 동작 |
| `ctest` | 5/5 통과 |
| validation | VUID/ERROR 없음 |

## 7. 후속

- 드라이버 수정은 확인하지 않았다. Mesa에 보고한다면 §4.2의 표가 최소 재현이 된다:
  색 부착물 하나와 `D32_SFLOAT` 깊이 부착물을 두고 `loadOp = Clear`로 render pass를
  한 번 제출한 뒤 `vkDeviceWaitIdle`이 반환되는지만 보면 된다.
- 깊이 전용 `D32_SFLOAT`가 반드시 필요해지면(스텐실 미사용 + 메모리 절감) 이 회피를
  되돌리기 전에 드라이버 상태를 다시 확인한다.
- `dzn`의 "not a conformant Vulkan implementation" 경고는 계속 나오지만, 현재 엔진 경로에는
  영향이 없다.

## 8. 참고

- [linux-porting-draft.md](../roadmap/linux-porting-draft.md) — L1~L2 검증 기록과 실행 명령
- [rhi.md](../design/rhi.md) §7 — GPU 오류를 복구 불가로 처리하는 정책
- [frame.md](../design/frame.md) — 프레임 수명과 present 소유권
- `src/nxt/graphics/backends/vulkan/frame/vulkan_fence.cpp` — 디바이스 로스트 보고 지점
