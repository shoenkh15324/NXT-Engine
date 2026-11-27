# Linux 포팅 임시 로드맵

> **목표**: WSL2와 WSLg에서 GCC·CMake·Ninja로 엔진을 빌드하고,
> Wayland에서 Sandbox를 실행한다. 코드 분석은 Linux용 clangd를 사용한다.

이 문서의 빌드 옵션과 `--window-system`은 아래 단계에서 구현할 계약이다.
엔진 빌드·실행 예시는 포팅 구현 이후의 절차이며, 현재 코드에서 성공했다고 검증한 명령이 아니다.

## 1. 범위와 진행 순서

| 단계 | 목표 | 완료 기준 |
|---|---|---|
| **L1 Linux 빌드 기반** (완료) | GCC 컴파일과 플랫폼 경계 정리 | 공통 코드와 테스트가 Linux에서 빌드됨 |
| **L2 Wayland 실행** (완료) | 엔진을 Wayland에서 실행 | Wayland에서 표시·configure·리사이즈·정상 종료 |
| **L3 선택과 회귀 검증** | Wayland 선택 정책과 Windows 유지 | 강제 선택, auto 정책, 오류와 Windows 회귀 검증 |
| **L4 개발 환경 고정** | 재현 가능한 빌드·clangd·측정 인계 | 프리셋·실행 결과·도구 버전·제한 기록 |

```text
L1 → L2(Wayland) → L3 → L4
```

Wayland 단일 백엔드로 진행한다.
작업 범위는 단일 창, 기존 삼각형 렌더링, 로그, 이벤트, 리사이즈, 종료다.
키보드·마우스의 전체 입력 체계, 멀티 윈도우, 전용 렌더 스레드와 애셋 로딩은 포함하지 않는다.

## 2. 현재 코드에서 필요한 변경

| 위치 | 현재 상태 | 작업 |
|---|---|---|
| `platform/backends/linux/` | 로그 sink·Wayland 창·createWindow 구현됨 | — |
| `platform/backends/CMakeLists.txt` | Windows·Linux 분기 등록됨 | — |
| `graphics/.../vulkan_instance_linux.cpp` | Wayland Surface 생성 구현됨 | — |
| `graphics/.../CMakeLists.txt` | Linux에서 Wayland 매크로·링크 사용 | — |
| `platform/window/window.hpp` | `NativeWindow{handle, display}`로 Wayland 전달 | — |
| `vulkan_swapchain.cpp` | 창 크기 기반 extent 선택·범위 제한·0 크기 처리 | — |
| `apps/sandbox/main.cpp` | OS별 로그 sink 선택 | `--window-system` 전달 (L3) |
| `apps/sandbox/CMakeLists.txt` | 플랫폼 배너를 OS별로 지정 | — |
| `tests/nxt/platform/` | Win32·POSIX sink 테스트를 OS별로 등록 | Wayland 창 계약 테스트(compositor 필요) |
| `CMakePresets.json` | Linux용 GCC 프리셋 없음 | 별도 Linux 빌드·테스트 프리셋 추가 |
| 로컬 VS Code 설정 | Windows 실행 파일·MSVC DB 경로 지정 | WSL용 실행 파일·GCC DB로 분리 |

`Renderer`, 셰이더, 일반적인 Vulkan 버퍼·이미지·파이프라인은 재사용한다.
단, GCC와 libstdc++ 컴파일 호환성, Vulkan 장치 기능·형식 지원은 실제 환경에서 검증한다.

## 3. 목표 디렉터리와 공통 계약

아래는 신규·변경 대상의 목표 구조다. 프로토콜 생성물은 소스 디렉터리가 아닌 빌드 디렉터리에 둔다.

```text
src/nxt/
├─ platform/
│  ├─ window/window.hpp
│  ├─ CMakeLists.txt                       xdg-shell 생성 규칙과 wayland-client 링크
│  └─ backends/
│     ├─ CMakeLists.txt
│     ├─ windows/                         기존 구현 유지
│     └─ linux/
│        ├─ linux_window.cpp               createWindow와 백엔드 선택
│        ├─ log_sink/
│        │  ├─ posix_console_log_sink.hpp
│        │  ├─ posix_console_log_sink.cpp
│        │  ├─ posix_file_log_sink.hpp
│        │  └─ posix_file_log_sink.cpp
│        └─ wayland/
│           ├─ wayland_window.hpp
│           └─ wayland_window.cpp
└─ graphics/backends/vulkan/core/
   ├─ vulkan_instance.hpp
   ├─ vulkan_instance.cpp
   ├─ vulkan_instance_win32.cpp
   └─ vulkan_instance_linux.cpp            Wayland Surface 생성
```

프로토콜 생성물(`xdg-shell-client-protocol.h`, `xdg-shell-protocol.c`)은 저장소가 아니라
빌드 디렉터리(`<build>/src/nxt/platform/wayland-generated/`)에 생성한다. 생성 규칙은
`platform/CMakeLists.txt`가 소유한다 — `add_custom_command`의 출력을 쓰는 타깃과 같은
디렉터리에 있어야 규칙이 타깃에 붙기 때문이다.

### 창 시스템 선택

- 실행 옵션은 `--window-system auto|wayland`로 정의한다. Windows에서는 auto 또는 Win32 기본 경로를 유지한다.
- `WindowDesc`가 선택 요청을 전달하고, 생성된 `NativeWindow`는 실제 시스템 종류와 핸들·연결 객체를 보관한다.
- `auto`는 Wayland 연결 가능 여부를 확인하고, 연결할 수 있으면 Wayland를 선택한다.
  연결이 불가능하면 이유를 출력하고 초기화를 실패시킨다.
- 강제 선택은 다른 백엔드로 조용히 바꾸지 않는다. Wayland 구현이 빌드되지 않았거나 연결에 실패하면 이유를 출력한다.
- Vulkan 초기화 이후의 장치·Surface 실패는 명확히 보고한다. 부분 초기화 객체를 둔 채 자동 전환하지 않는다.

### Vulkan Surface 확장

현재 `platformSurfaceExtension()`의 무인자 계약을 실제 NativeWindow 종류를 받아 결정하는 계약으로 바꾼다.
Engine은 이미 Window를 먼저 생성하므로, 선택한 종류를 Instance 생성 설정까지 전달할 수 있다.
RHI에 Vulkan 확장 이름을 노출하지 않고 플랫폼 종류만 전달한다.

- Wayland 창: `VK_KHR_surface` + `VK_KHR_wayland_surface`.
- 실행 시에는 선택한 Surface 확장만 요구한다.
- graphics는 창 연결을 소유하지 않는다. Surface가 살아 있는 동안 native 연결·창 객체를 유지한다.

## 4. L1 — GCC 빌드 기반

### L1-1 플랫폼 공통 경계 (완료)

- Linux 로그 sink를 구현하고 Sandbox의 Win32 직접 의존을 OS별 선택으로 바꾼다.
- 로그 경로는 실행 파일 위치 기준 등 기존 계약을 유지하고 현재 작업 디렉터리에 따라 바뀌지 않게 한다.
- `std::thread` 사용에 필요한 CMake `Threads::Threads` 연결을 확인한다.
- Win32 테스트는 Windows에서만 등록한다. Linux에서 공통 테스트가 빠지지 않게 한다.
- Linux Wayland 의존성은 `pkg-config` 등으로 발견하고 절대 개인 경로를 저장소에 넣지 않는다.
- GCC에서 경고·표준 라이브러리·대소문자 경로 문제를 수정한다.

**완료 조건:** GCC가 core·math·engine 공통 코드를 컴파일하고 Linux에서 실행 가능한 공통 테스트가 통과한다.
공통 테스트만 빌드하더라도 현재 루트 CMake는 Vulkan을 필수로 찾는다는 점을 반영한다.

## 5. L2 — Wayland 포팅

### L2-1 프로토콜 생성과 빌드 (완료)

- `wayland-client`와 `wayland-scanner`, `wayland-protocols`를 발견한다.
- 설치된 `xdg-shell.xml`에서 클라이언트 헤더와 프로토콜 소스를 빌드 디렉터리에 생성한다.
- 생성 소스를 C로 컴파일하면 CMake에서 Linux Wayland 경로에 C 언어를 활성화한다.
  현재 루트의 `LANGUAGES CXX`만으로 생성 C 파일을 그대로 추가하지 않는다.
- 생성 파일 경로·의존성·재생성 규칙을 CMake에 명시한다. 생성 파일은 커밋하지 않는다.

### L2-2 창과 이벤트 수명 (완료)

- display·registry에 연결하고 compositor·xdg-shell의 필요한 인터페이스를 바인딩한다.
- wl_surface → xdg_surface → xdg_toplevel을 만들고 제목과 app ID를 설정한다.
- 최초 configure를 받아 acknowledge한 뒤 렌더링 가능한 상태로 전환한다.
- configure의 크기가 0일 때는 클라이언트가 정할 크기를 사용한다. 이를 곧바로 최소화 상태라고 해석하지 않는다.
- 리사이즈의 논리 크기와 실제 buffer 픽셀 크기를 구분한다. 초기 범위는 정수 scale을 처리하고 fractional scaling은 제한으로 기록한다.
- pending 이벤트 처리·flush·읽기 준비를 조합해 이벤트 루프를 구성하고 메인 프레임을 무기한 대기시키지 않는다.
- close 요청과 연결 오류를 처리하고 Surface → Wayland 객체 → display 연결 순으로 정리한다.
- Wayland는 최소화 여부를 Win32와 같은 방식으로 알려준다고 가정하지 않는다. 사용 가능한 상태 정보에 맞춰 정책을 둔다.

### L2-3 Swapchain과 extent (완료)

- Surface가 고정 크기를 제공하면 이를 사용한다.
- `currentExtent`가 미지정 값이면 최신 창 크기를 사용하고 `minImageExtent`·`maxImageExtent` 안으로 제한한다.
- 크기 0에서는 acquire와 깊이 자원 재생성을 보류한다. 엔진 루프는 크기 0에서 프레임과
  재생성을 부르지 않고 16ms 쉬며, 스왑체인은 이전 extent를 유지한다.
- 창 크기와 실제 선택 extent의 차이 때문에 매 프레임 재생성하지 않도록 dirty 상태·비교 기준을 정한다.
- 기존 fence·semaphore·ImageView·Framebuffer 파괴 순서를 유지한다.

### L2-4 Vulkan 연결 (완료)

- NativeWindow에 wl_display·wl_surface를 담고 `VK_KHR_wayland_surface`로 Surface를 생성한다.
- extent·프레임 자원 재생성 경로는 L2-3의 공통 경로를 사용한다.
- xdg-shell configure 처리와 Swapchain 갱신이 충돌하지 않는지 검증한다.

**게이트 Wayland:** 강제 Wayland 실행에서 초기 configure, 표시, 리사이즈, 닫기, 정상 종료가 검증된다.

## 6. L3 — 빌드 옵션과 실행 선택 검증

### 구현할 빌드 옵션

| 옵션 | 기본값 | 의미 |
|---|---|---|
| `NXT_LINUX_ENABLE_WAYLAND` | ON | Linux Wayland 백엔드 포함 |

- 옵션은 Linux에서만 적용한다. OFF인 Linux GUI 빌드는 configure 단계에서 명확히 거부한다.
- ON인 백엔드의 개발 패키지가 없으면 필요한 패키지를 설명하며 configure를 실패시킨다.
- 런타임 창 시스템 선택은 3장의 계약을 따른다. 실제 선택 결과를 로그에 출력한다.
- Windows의 컴파일러·창 백엔드·Vulkan Surface 경로를 변경 없이 사용할 수 있게 유지한다.

### 검증 매트릭스

| 빌드 | 실행 요청 | 기대 결과 |
|---|---|---|
| Wayland | wayland | Wayland 창과 Surface로 표시 |
| Wayland | auto | Wayland 연결 선택 및 로그 |
| Wayland OFF | wayland | 미포함 백엔드라는 명확한 오류 |
| Wayland | 사용할 display 연결 없음 | 초기화 실패를 보고하고 정상 정리 |
| Windows MSVC | 기존 실행 | 기존 Sandbox와 테스트 정상 |

Wayland에서 Debug+validation, RelWithDebInfo 실행을 확인한다.
GPU와 화면 표시 경로가 다를 수 있으므로 Windows와 WSL의 fps 차이를 곧바로 엔진 회귀로 판단하지 않는다.

## 7. WSL에서 빌드와 실행

### 7.1 작업 디렉터리

Windows 체크아웃과 별도로 WSL의 Linux 파일시스템에 체크아웃을 두는 것을 권장한다.
Windows의 빌드 캐시·SDK·로컬 `.vscode` 경로를 공유하지 않도록 한다.
WSL 체크아웃에도 작업할 커밋이 실제로 반영됐는지 확인한다.

```bash
cd ~/src/NXT-Engine
code .
```

이 경로는 준비한 체크아웃 위치로 바꾼다. 기존 디렉터리가 있다고 가정해 복제나 덮어쓰기를 자동 수행하지 않는다.
VS Code 좌측 아래가 `WSL: 배포판`으로 표시돼야 한다.
[VS Code WSL 개발 안내](https://code.visualstudio.com/docs/remote/wsl)

### 7.2 Debug 빌드

아래는 L1~L3 구현 후 저장소 루트에서 실행한다. Vulkan SDK 환경이 활성화된 터미널을 사용한다.

```bash
cmake -S . -B build/linux-debug -G Ninja \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-14 \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++-14 \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DNXT_BUILD_SANDBOX=ON -DNXT_BUILD_TESTS=ON \
    -DNXT_LINUX_ENABLE_WAYLAND=ON
cmake --build build/linux-debug --parallel
ctest --test-dir build/linux-debug --output-on-failure
```

최초 configure에서 doctest를 받기 위한 네트워크 접근이 필요할 수 있다.
Configure가 실패했으면 바로 build하지 말고 먼저 원인을 해결한다.
컴파일러·SDK를 교체하면 같은 캐시를 재사용하지 말고 새 빌드 디렉터리 또는 `cmake --fresh`로 다시 구성한다.

```bash
./build/linux-debug/apps/sandbox/sandbox \
    --window-system wayland --objects 1000 --culling on --log-level info
```

창을 닫은 뒤 다음 명령을 실행한다. 처음에는 작은 입력으로 기능을 확인하고 그다음 10만 오브젝트로 확장한다.

### 7.3 RelWithDebInfo 빌드

```bash
cmake -S . -B build/linux-bench -G Ninja \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-14 \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++-14 \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DNXT_BUILD_SANDBOX=ON -DNXT_BUILD_TESTS=OFF \
    -DNXT_LINUX_ENABLE_WAYLAND=ON
cmake --build build/linux-bench --parallel
./build/linux-bench/apps/sandbox/sandbox \
    --window-system wayland --objects 100000 --culling on --present immediate --log-level info
```

요청한 present 방식이 미지원이면 실제 선택한 방식도 기록한다.
WSLg 표시 경로를 포함한 결과이므로 네이티브 Linux나 Windows의 기준선을 대체하지 않는다.

### 7.4 프리셋 추가

L4에서 `linux-debug`·`linux-bench` configure/build 프리셋과 Debug용 test 프리셋을 추가한다.
단일 구성 Ninja이므로 configure의 `CMAKE_BUILD_TYPE`을 반드시 지정한다.
Windows MSVC 프리셋과 빌드 출력 경로를 공유하지 않고 Linux GCC를 지정한다.
현재 이 프리셋은 미구현이므로 위의 직접 configure 절차를 구현 검증의 기준으로 사용한다.

## 8. WSL의 VS Code와 clangd

빌드는 GCC가 담당하고, clangd는 GCC의 `compile_commands.json`을 읽어 분석한다.
clangd를 사용해도 빌드 컴파일러가 Clang으로 바뀌는 것은 아니다.

1. Windows VS Code에 WSL 확장을 설치하고 대상 체크아웃을 WSL로 연다.
2. CMake Tools와 clangd 확장을 WSL 측에 설치한다.
3. Linux Debug configure를 먼저 성공시켜 `build/linux-debug/compile_commands.json`을 생성한다.
4. **WSL 전용 체크아웃의 로컬 설정** `.vscode/settings.json`에 다음을 설정한다.
   Windows 체크아웃의 같은 파일은 그대로 둔다.

```json
{
    "cmake.cmakePath": "/usr/bin/cmake",
    "cmake.useCMakePresets": "never",
    "cmake.generator": "Ninja",
    "cmake.buildDirectory": "${workspaceFolder}/build/linux-debug",
    "cmake.buildType": "Debug",
    "cmake.configureSettings": {
        "CMAKE_C_COMPILER": "/usr/bin/gcc-14",
        "CMAKE_CXX_COMPILER": "/usr/bin/g++-14",
        "CMAKE_MAKE_PROGRAM": "/usr/bin/ninja",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "NXT_BUILD_TESTS": "ON",
        "NXT_LINUX_ENABLE_WAYLAND": "ON"
    },
    "clangd.path": "/usr/bin/clangd-18",
    "clangd.arguments": [
        "--compile-commands-dir=${workspaceFolder}/build/linux-debug",
        "--query-driver=/usr/bin/g++-14",
        "--background-index"
    ],
    "C_Cpp.intelliSenseEngine": "disabled"
}
```

이 설정은 프리셋 추가 전에도 CMake Tools에서 Debug 구성을 사용할 수 있도록 한다.
L4 이후에는 `cmake.useCMakePresets`를 `always`로 바꾸고 Linux configure 프리셋을 선택한다.
SDK 환경 변수가 필요하면 설정한 WSL 터미널에서 VS Code를 시작하고 configure 로그에서 실제 SDK 경로를 확인한다.
도구 설치 위치가 다르면 `command -v`의 결과에 맞춘다.

`--query-driver`는 지정한 GCC에서 표준 헤더 검색 경로 등을 얻기 위한 실행 허용 목록이다.
신뢰하는 실제 컴파일러만 지정한다.
[clangd compile commands 설명](https://clangd.llvm.org/design/compile-commands)

### 분석 확인

```bash
clangd-18 --check=src/nxt/engine/engine.cpp \
    --compile-commands-dir=build/linux-debug \
    --query-driver=/usr/bin/g++-14
```

VS Code에서 `clangd: Restart language server` 실행 후 Output의 clangd 로그에서 Linux DB와 GCC 경로를 확인한다.
Windows의 `build/msvc`, `cl.exe`, `C:/...`를 읽으면 설정이 섞인 것이다.
상위 디렉터리와 기존 `.clangd`의 DB 지정도 확인하고, 최종적으로 읽은 DB를 기준으로 판단한다.
GCC 빌드가 성공하고 분석만 실패하면 clangd 버전과 표준 헤더 호환성을 확인한다.
진단을 일괄 비활성화해 해결한 것으로 처리하지 않는다.

## 9. 완료 조건

- [ ] WSL2·WSLg·GCC·CMake·Ninja·clangd·SDK와 실제 Vulkan 장치를 기록했다.
- [x] Linux Debug 빌드와 공통 테스트가 성공한다.
- [x] Wayland에서 Sandbox를 표시한다.
- [ ] 강제 선택·auto·미포함·연결 실패 결과가 명확히 로그에 나온다.
- [ ] 연속 리사이즈, 최소화/복귀에 해당하는 조작, 닫기와 초기화 도중 실패를 검증했다.
- [x] Debug validation 활성화를 확인했고 ERROR/VUID가 없다.
- [x] RelWithDebInfo의 CPU/GPU/fence/draws가 나오며 timestamp 미지원을 구분한다.
- [ ] clangd가 Linux compile DB로 앱과 Wayland 백엔드를 분석한다.
- [ ] Windows MSVC 빌드·실행·테스트가 유지된다.
- [x] 네이티브 Linux 미검증, fractional scaling 등 지원 제한을 기록했다.

L4에서 새 검증 기록을 만들고 실행 명령·결과·제한을 정리한다.

### L1~L2 검증 기록 (2026-11-27, WSLg)

| 항목 | 값 |
|---|---|
| 환경 | WSL2 + WSLg. `WAYLAND_DISPLAY=wayland-0`(WSLg 런타임 디렉터리 심볼릭 링크) |
| 빌드 도구 | GCC 13.3.0, CMake 3.28.3, Ninja, `-Wall -Wextra` |
| Wayland | wayland-client 1.22.0, wayland-protocols 1.45, `/usr/bin/wayland-scanner` |
| Vulkan | 헤더·런타임 1.3.275, ICD `dzn`(D3D12) · `lavapipe` |
| 생성물 | `<build>/src/nxt/platform/wayland-generated/xdg-shell-{client-protocol.h,protocol.c}` |

확인한 것:

- 기본 빌드·Debug·RelWithDebInfo 빌드 모두 경고 없이 `nxt_platform`·`nxt_graphics`·`nxt_engine`·`sandbox` 링크.
- `ctest` 5/5 통과. 플랫폼 테스트는 Linux에서 POSIX sink 20케이스(83 단언)를 실행하고,
  Win32 테스트는 등록하지 않는다.
- Wayland 창: 최초 configure 1280x720, `scale 1`, `VK_KHR_wayland_surface` 선택.
- Swapchain: 이미지 4장, `B8G8R8A8Srgb`, `Fifo`, extent 1280x720.
- 1000 오브젝트 실행: 약 59fps, validation ERROR/VUID 없음. 프레임 요약에 cpu/gpu/fence/draws가 나온다.
- 100000 오브젝트 실행: draws=65025, dzn(실제 GPU 경로)에서 초당 9프레임 수준, 디바이스 로스트 없음.
- RelWithDebInfo + 100000 오브젝트: draws=65025, llvmpipe에서 약 12fps(소프트웨어 렌더러).

검증하지 못한 것: 대화형 리사이즈·최소화/복귀·창 닫기(자동 조작 불가), fractional scaling,
`--window-system` 정책(L3), clangd 분석, Windows MSVC 회귀, 네이티브 Linux.

**WSLg dzn의 D32_SFLOAT 깊이 clear 버그:** 기본 ICD인 `dzn`(Vulkan-on-D3D12)은
`VK_FORMAT_D32_SFLOAT` 깊이 부착물을 render pass에서 clear할 때 GPU를 멈춘다. 그러면 약
4초 뒤 `VK_ERROR_DEVICE_LOST`로 디바이스가 제거되고, 엔진은 그 원인을 남기고 종료한다
(`vulkan_fence.cpp`의 `waitFence`가 `ErrorDeviceLost`를 보고). 재현 조건을 나눠 확인한 결과:

| 깊이 처리 | 결과 |
|---|---|
| 깊이 부착물 없음(색만 clear) | 정상 |
| `D32_SFLOAT` + `loadOp = DontCare` | 정상 |
| `D32_SFLOAT` + `loadOp = Clear` | 디바이스 로스트 |
| `D32_SFLOAT` + `loadOp = Clear` + `storeOp = Store` | 디바이스 로스트 |
| `D24_UNORM_S8_UINT` + `loadOp = Clear` | 정상 |
| `D32_SFLOAT_S8_UINT` + `loadOp = Clear` | 정상 |

그래서 깊이 형식을 `D32SFloatS8Uint`로 고정했다(`FrameLoop::kDepthFormat`). 깊이 정밀도는
32비트로 같고 스텐실은 아직 쓰지 않는다. 위 실행 결과는 dzn에서 얻었으며 lavapipe로도 같은
동작을 확인했다. 깊이 전용 `D32_SFLOAT`를 계속 쓰려면 이 드라이버가 고쳐져야 한다.
분석 과정과 최소 재현은 [issues/0001-dzn-d32-sfloat-depth-clear.md](../issues/0001-dzn-d32-sfloat-depth-clear.md)에 정리했다.

```bash
# WSLg 기본 경로 (dzn)
./build/linux-debug/apps/sandbox/sandbox --objects 1000 --log-level info

# 소프트웨어 렌더러로 비교할 때
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json \
    ./build/linux-debug/apps/sandbox/sandbox --objects 1000 --log-level info
```

**Vulkan 헤더 버전:** 배포판 헤더가 1.3이면 `vk::ApiVersion14`가 없으므로 1.3으로 요청하고
경고를 남긴다(`vulkan_instance.cpp`). 1.4 SDK를 설치하면 `VK_API_VERSION_1_4`가 정의되어
경고 없이 1.4로 요청한다.

### 작업량 예상

Wayland 표시까지 1~3일, 선택·회귀 검증과 개발 환경 고정을 포함해 전체 3~7작업일을 예상한다.
구현 경험과 환경에 따라 달라지는 추정이며 드라이버 구축·API 요구 변경이 필요하면 별도 산정한다.

## 10. 관련 자료

- [RHI 설계](../design/rhi.md) — Vulkan을 격리하는 경계
- [프레임 설계](../design/frame.md) — 동기화와 자원 파괴
- [Microsoft WSLg](https://github.com/microsoft/wslg) — Wayland 실행 환경
- [VS Code WSL](https://code.visualstudio.com/docs/remote/wsl) — Linux 측 확장·도구 실행
- [clangd 설정](https://clangd.llvm.org/config) — compile DB와 분석 설정
