# Linux 포팅 임시 로드맵

> **목표**: WSL2와 WSLg에서 GCC·CMake·Ninja로 엔진을 빌드하고,
> XCB와 Wayland 각각에서 Sandbox를 실행한다. 코드 분석은 Linux용 clangd를 사용한다.

이 문서의 빌드 옵션과 `--window-system`은 아래 단계에서 구현할 계약이다.
환경 점검 명령을 제외한 엔진 빌드·실행 예시는 포팅 구현 이후의 절차이며, 현재 코드에서 성공했다고 검증한 명령이 아니다.

## 1. 범위와 진행 순서

| 단계 | 목표 | 완료 기준 |
|---|---|---|
| **L0 환경 확인** | WSLg와 Vulkan 실행 조건 확인 | GUI 연결, 드라이버·API·Surface 확장·컴파일 도구 확인 |
| **L1 Linux 빌드 기반** | GCC 컴파일과 플랫폼 경계 정리 | 공통 코드와 테스트가 Linux에서 빌드됨 |
| **L2 XCB 실행** | 가장 먼저 Sandbox 창과 렌더링 확보 | XCB에서 표시·리사이즈·닫기·정상 종료 |
| **L3 Wayland 실행** | 같은 엔진을 Wayland에서도 실행 | Wayland에서 표시·configure·리사이즈·정상 종료 |
| **L4 선택과 회귀 검증** | 두 백엔드를 선택하고 Windows 유지 | 양쪽 강제 선택, auto 정책, 오류와 Windows 회귀 검증 |
| **L5 개발 환경 고정** | 재현 가능한 빌드·clangd·측정 인계 | 프리셋·실행 결과·도구 버전·제한 기록 |

```text
Phase 1-B1 → L0 → L1 → L2(XCB) → L3(Wayland) → L4 → L5 → Phase 1-B2
```

XCB를 먼저 완성해 공통 문제와 Wayland 고유 문제를 분리한다.
작업 범위는 단일 창, 기존 삼각형 렌더링, 로그, 이벤트, 리사이즈, 종료다.
키보드·마우스의 전체 입력 체계, 멀티 윈도우, 전용 렌더 스레드와 애셋 로딩은 포함하지 않는다.

## 2. 현재 코드에서 필요한 변경

| 위치 | 현재 상태 | 작업 |
|---|---|---|
| `platform/backends/linux/` | CMake 파일만 있고 내용은 비어 있음 | Linux 공통 로그와 XCB·Wayland 창 구현 |
| `platform/backends/CMakeLists.txt` | Windows 소스만 등록 | Linux 분기와 의존성 등록 |
| `graphics/.../vulkan_instance_linux.cpp` | 빈 파일 | 선택한 창 시스템에 맞는 Surface 생성 |
| `graphics/.../CMakeLists.txt` | Linux에서 XCB 매크로만 정의 | 빌드된 백엔드에 맞는 Surface 구현·매크로·링크 |
| `platform/window/window.hpp` | 핸들과 display만 보관 | NativeWindow에 창 시스템 종류 추가 |
| `vulkan_swapchain.cpp` | Surface가 크기를 지정하지 않으면 종료 | 창 크기 기반 extent 선택·범위 제한·0 크기 처리 |
| `apps/sandbox/main.cpp` | Win32 로그 sink 직접 사용 | Linux 로그 선택 및 창 시스템 CLI 전달 |
| `apps/sandbox/CMakeLists.txt` | 플랫폼 배너가 Windows로 고정 | 플랫폼별 값 지정 |
| `tests/nxt/platform/` | Win32 테스트가 무조건 등록됨 | OS별 테스트 등록과 Linux 계약 테스트 |
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
│  └─ backends/
│     ├─ CMakeLists.txt
│     ├─ windows/                         기존 구현 유지
│     └─ linux/
│        ├─ CMakeLists.txt
│        ├─ linux_window.cpp               createWindow와 백엔드 선택
│        ├─ log_sink/
│        │  ├─ linux_console_log_sink.hpp
│        │  ├─ linux_console_log_sink.cpp
│        │  ├─ linux_file_log_sink.hpp
│        │  └─ linux_file_log_sink.cpp
│        ├─ xcb/
│        │  ├─ xcb_window.hpp
│        │  └─ xcb_window.cpp
│        └─ wayland/
│           ├─ wayland_window.hpp
│           ├─ wayland_window.cpp
│           └─ protocols/CMakeLists.txt    xdg-shell 코드 생성
└─ graphics/backends/vulkan/core/
   ├─ vulkan_instance.hpp
   ├─ vulkan_instance.cpp
   ├─ vulkan_instance_win32.cpp
   ├─ vulkan_instance_linux.cpp            Surface 생성 분기
   ├─ vulkan_surface_xcb.cpp
   └─ vulkan_surface_wayland.cpp
```

### 창 시스템 선택

- 실행 옵션은 `--window-system auto|xcb|wayland`로 정의한다. Windows에서는 auto 또는 Win32 기본 경로를 유지한다.
- `WindowDesc`가 선택 요청을 전달하고, 생성된 `NativeWindow`는 실제 시스템 종류와 핸들·연결 객체를 보관한다.
- `auto`는 Wayland 연결 가능 여부를 먼저 확인하고, 초기 창 생성이 불가능하면 XCB를 시도한다.
  환경 변수의 존재만으로 성공을 판정하지 않는다.
- 강제 선택은 다른 백엔드로 조용히 바꾸지 않는다. 선택한 구현이 빌드되지 않았거나 연결에 실패하면 이유를 출력한다.
- Vulkan 초기화 이후의 장치·Surface 실패는 명확히 보고한다. 부분 초기화 객체를 둔 채 다른 백엔드로 자동 전환하지 않는다.

### Vulkan Surface 확장

현재 `platformSurfaceExtension()`의 무인자 계약을 실제 NativeWindow 종류를 받아 결정하는 계약으로 바꾼다.
Engine은 이미 Window를 먼저 생성하므로, 선택한 종류를 Instance 생성 설정까지 전달할 수 있다.
RHI에 Vulkan 확장 이름을 노출하지 않고 플랫폼 종류만 전달한다.

- XCB 창: `VK_KHR_surface` + `VK_KHR_xcb_surface`.
- Wayland 창: `VK_KHR_surface` + `VK_KHR_wayland_surface`.
- 컴파일 시 두 구현을 포함해도 실행 시에는 선택한 Surface 확장만 요구한다.
- graphics는 창 연결을 소유하지 않는다. Surface가 살아 있는 동안 native 연결·창 객체를 유지한다.

## 4. L0 — WSL2와 도구 준비

### 4.1 Windows에서 확인

PowerShell에서 실행한다.

```powershell
wsl --version
wsl --list --verbose
```

대상 배포판이 Version 2인지 확인한다. WSLg가 오래됐거나 GUI 지원이 없으면 작업 중인 WSL 세션을 정리한 뒤 업데이트한다.

```powershell
wsl --update
wsl --shutdown
```

`wsl --shutdown`은 실행 중인 모든 WSL 배포판을 종료한다. 업데이트 후 배포판을 다시 시작한다.
WSLg는 X11·Wayland 창을 Windows에 표시한다. 별도의 Linux 데스크톱 전체나 Windows용 X 서버를 먼저 설치할 필요는 없다.
[Microsoft WSL GUI 안내](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps)

### 4.2 Linux 도구 설치

명령 예시는 **Ubuntu 24.04 LTS, x86_64** 기준이다. 다른 배포판은 패키지 이름과 버전을 확인한다.
기준 컴파일러는 GCC/G++ 14, 분석기는 clangd 18 이상으로 잡되 실제 표준 라이브러리 분석이 되는지 확인한다.
G++ 14와 clangd 18은 Ubuntu 24.04 패키지로 제공된다.
[G++ 패키지](https://packages.ubuntu.com/en/g%2B%2B-14), [clangd 패키지](https://packages.ubuntu.com/noble/clangd-18)

WSL의 Bash에서 실행한다.

```bash
sudo apt update
sudo apt install build-essential gcc-14 g++-14 cmake ninja-build pkg-config git \
    clangd-18 gdb libxcb1-dev libwayland-dev libwayland-bin wayland-protocols \
    libvulkan-dev vulkan-tools vulkan-validationlayers glslang-tools
```

저장소 구성에 따라 universe 활성화가 필요할 수 있다. 패키지를 찾지 못하면 배포판 버전과 `apt-cache policy`를 확인한다.
GCC·clangd·CMake는 Linux 실행 파일을 사용하고 Windows의 `cmake.exe`·`clangd.exe`를 호출하지 않는다.

```bash
command -v g++-14 cmake ninja clangd-18 glslangValidator vulkaninfo
g++-14 --version
cmake --version
ninja --version
clangd-18 --version
pkg-config --modversion xcb wayland-client wayland-protocols
wayland-scanner --version
```

CMake는 프로젝트의 최소 요구인 3.25 이상이어야 한다.
현재 코드가 쓰는 `<format>` 등을 작은 컴파일 확인과 실제 core 빌드로 검증한다.
TSan은 후속 Phase 1-B 동시성 작업에서 사용하며 포팅 완료의 선행 조건으로 늘리지 않는다.

### 4.3 Vulkan SDK와 실행 드라이버 구분

**SDK 설치와 Vulkan GPU 실행 가능 여부는 별개다.**
위 apt 패키지는 기본 도구를 준비하지만 배포판의 Vulkan 헤더가 프로젝트 요구보다 오래될 수 있다.
현재 엔진은 `vk::ApiVersion14`를 사용하고 Vulkan 1.4를 요청한다.

1. Linux용 Vulkan SDK 1.4 이상에서 필요한 Vulkan-Hpp 헤더와 도구를 준비한다.
2. [LunarG 공식 SDK](https://vulkan.lunarg.com/sdk/home/)에서 Linux 패키지를 선택하고 해당 버전 설치 안내를 따른다.
3. tarball 방식이라면 추출한 버전 디렉터리의 `setup-env.sh`를 같은 터미널에서 source한다.
   실제 경로는 설치 위치를 사용한다. Windows SDK 경로를 넘기지 않는다.
4. source 후 다시 `command -v glslangValidator vulkaninfo`를 확인한다. CMake가 사용할 헤더·로더·도구 경로도 기록한다.

SDK 환경 설정 방식은 [LunarG Linux 설치 안내](https://vulkan.lunarg.com/doc/view/1.4.328.1/linux/getting_started.html)를 참고하고,
실제 설치 버전의 설명을 우선한다.

### 4.4 GUI와 Vulkan 실행 게이트

```bash
printf 'DISPLAY=%s\nWAYLAND_DISPLAY=%s\nXDG_RUNTIME_DIR=%s\n' \
    "$DISPLAY" "$WAYLAND_DISPLAY" "$XDG_RUNTIME_DIR"
vulkaninfo --summary
vulkaninfo > /tmp/nxt-vulkaninfo.txt
vkcube --help
```

- 환경 변수와 실제 소켓·연결을 확인한다. WSLg가 설정한 값을 임의의 고정 IP로 덮어쓰지 않는다.
- `vulkaninfo`에서 장치 이름·유형, 장치 API 버전, Surface 확장, 표시 지원을 확인한다.
- 설치된 도구의 도움말을 보고 XCB·Wayland WSI를 각각 선택해 시험한다. `vkcube`의 WSI 선택 옵션은 빌드에 따라 다를 수 있다.
- CPU 소프트웨어 렌더러만 보이면 기능 확인과 GPU 가속 성공을 구분해 기록한다.
- 엔진의 Vulkan 1.4 요청과 필요한 device 기능·형식을 지원하지 못하면, SDK만 다시 설치해 해결됐다고 판단하지 않는다.
  Windows GPU 드라이버, WSL, Linux ICD/Mesa 구성과 실제 API 지원을 먼저 확인한다.
- 1.4 지원이 불가능하면 지원 API 하향을 별도 결정으로 남긴다. 필요한 기능을 조사하지 않고 버전 숫자만 낮추지 않는다.

**L0 완료 조건:** 도구 버전, WSI별 테스트 결과, 실제 렌더러와 API 지원을 기록한다.
환경이 막히면 CPU 빌드 포팅은 진행할 수 있지만 GUI·GPU 실행 완료로 표시하지 않는다.

## 5. L1과 L2 — GCC 빌드와 XCB 우선 포팅

### L1-1 플랫폼 공통 경계

- Linux 로그 sink를 구현하고 Sandbox의 Win32 직접 의존을 OS별 선택으로 바꾼다.
- 로그 경로는 실행 파일 위치 기준 등 기존 계약을 유지하고 현재 작업 디렉터리에 따라 바뀌지 않게 한다.
- `std::thread` 사용에 필요한 CMake `Threads::Threads` 연결을 확인한다.
- Win32 테스트는 Windows에서만 등록한다. Linux에서 공통 테스트가 빠지지 않게 한다.
- Linux XCB·Wayland 의존성은 `pkg-config` 등으로 발견하고 절대 개인 경로를 저장소에 넣지 않는다.
- GCC에서 경고·표준 라이브러리·대소문자 경로 문제를 수정한다.

**완료 조건:** GCC가 core·math·engine 공통 코드를 컴파일하고 Linux에서 실행 가능한 공통 테스트가 통과한다.
공통 테스트만 빌드하더라도 현재 루트 CMake는 Vulkan을 필수로 찾는다는 점을 반영한다.

### L2-1 XCB 창과 Surface

- XCB 연결·창 생성·표시, 제목, 닫기 요청, 크기 변경, expose 이벤트를 처리한다.
- 이벤트 폴링은 메시지가 없다고 프레임을 무기한 대기시키지 않는다.
- `WM_DELETE_WINDOW` 처리와 창 파괴의 순서를 정한다. 닫기 요청은 우선 상태로 반영하고
  GPU·Surface 정리 전에 native 연결을 먼저 끊지 않도록 한다.
- NativeWindow는 XCB 연결과 창 ID를 전달하고, Vulkan 쪽에서 XCB Surface를 만든다.
- 연결 실패·Surface 생성 실패·미지원 present를 식별 가능한 로그로 남긴다.

### L2-2 Swapchain과 extent

- Surface가 고정 크기를 제공하면 이를 사용한다.
- `currentExtent`가 미지정 값이면 최신 창 크기를 사용하고 `minImageExtent`·`maxImageExtent` 안으로 제한한다.
- 크기 0에서는 acquire와 깊이 자원 재생성을 보류한다. 현재 `FrameLoop::recreate()`의 무조건 후속 생성 경로도 함께 정리한다.
- 창 크기와 실제 선택 extent의 차이 때문에 매 프레임 재생성하지 않도록 dirty 상태·비교 기준을 정한다.
- 기존 fence·semaphore·ImageView·Framebuffer 파괴 순서를 유지한다.

**게이트 XCB:** 창 표시 → 기존 삼각형·컬링 표시 → 반복 리사이즈 → 닫기 → 정상 종료가 된다.
Debug 검증 레이어가 실제 활성화된 실행에서 ERROR/VUID가 없어야 한다.

## 6. L3 — Wayland 포팅

### L3-1 프로토콜 생성과 빌드

- `wayland-client`와 `wayland-scanner`, `wayland-protocols`를 발견한다.
- 설치된 `xdg-shell.xml`에서 클라이언트 헤더와 프로토콜 소스를 빌드 디렉터리에 생성한다.
- 생성 소스를 C로 컴파일하면 CMake에서 Linux Wayland 경로에 C 언어를 활성화한다.
  현재 루트의 `LANGUAGES CXX`만으로 생성 C 파일을 그대로 추가하지 않는다.
- 생성 파일 경로·의존성·재생성 규칙을 CMake에 명시한다. 생성 파일은 커밋하지 않는다.

### L3-2 창과 이벤트 수명

- display·registry에 연결하고 compositor·xdg-shell의 필요한 인터페이스를 바인딩한다.
- wl_surface → xdg_surface → xdg_toplevel을 만들고 제목과 app ID를 설정한다.
- 최초 configure를 받아 acknowledge한 뒤 렌더링 가능한 상태로 전환한다.
- configure의 크기가 0일 때는 클라이언트가 정할 크기를 사용한다. 이를 곧바로 최소화 상태라고 해석하지 않는다.
- 리사이즈의 논리 크기와 실제 buffer 픽셀 크기를 구분한다. 초기 범위는 정수 scale을 처리하고 fractional scaling은 제한으로 기록한다.
- pending 이벤트 처리·flush·읽기 준비를 조합해 이벤트 루프를 구성하고 메인 프레임을 무기한 대기시키지 않는다.
- close 요청과 연결 오류를 처리하고 Surface → Wayland 객체 → display 연결 순으로 정리한다.
- Wayland는 최소화 여부를 Win32와 같은 방식으로 알려준다고 가정하지 않는다. 사용 가능한 상태 정보에 맞춰 정책을 둔다.

### L3-3 Vulkan 연결

- NativeWindow에 wl_display·wl_surface를 담고 `VK_KHR_wayland_surface`로 Surface를 생성한다.
- XCB에서 정리한 extent·프레임 자원 재생성 경로를 공통으로 사용한다.
- xdg-shell configure 처리와 Swapchain 갱신이 충돌하지 않는지 검증한다.

**게이트 Wayland:** 강제 Wayland 실행에서 초기 configure, 표시, 리사이즈, 닫기, 정상 종료가 검증된다.
XCB 실행이 동시에 회귀하지 않아야 한다.

## 7. L4 — 빌드 옵션과 실행 선택 검증

### 구현할 빌드 옵션

| 옵션 | 기본값 | 의미 |
|---|---|---|
| `NXT_LINUX_ENABLE_XCB` | ON | Linux XCB 백엔드 포함 |
| `NXT_LINUX_ENABLE_WAYLAND` | ON | Linux Wayland 백엔드 포함 |

- 옵션은 Linux에서만 적용한다. 둘 다 OFF인 Linux GUI 빌드는 configure 단계에서 명확히 거부한다.
- ON인 백엔드의 개발 패키지가 없으면 필요한 패키지를 설명하며 configure를 실패시킨다.
- 런타임 창 시스템 선택은 3장의 계약을 따른다. 실제 선택 결과를 로그에 출력한다.
- Windows의 컴파일러·창 백엔드·Vulkan Surface 경로를 변경 없이 사용할 수 있게 유지한다.

### 검증 매트릭스

| 빌드 | 실행 요청 | 기대 결과 |
|---|---|---|
| XCB+Wayland | xcb | XCB 창과 Surface로 표시 |
| XCB+Wayland | wayland | Wayland 창과 Surface로 표시 |
| XCB+Wayland | auto | 실제 연결 가능한 백엔드 선택 및 로그 |
| XCB 전용 | wayland | 미포함 백엔드라는 명확한 오류 |
| Wayland 전용 | xcb | 미포함 백엔드라는 명확한 오류 |
| XCB+Wayland | 사용할 display 연결 없음 | 초기화 실패를 보고하고 정상 정리 |
| Windows MSVC | 기존 실행 | 기존 Sandbox와 테스트 정상 |

각 WSI에서 Debug+validation, RelWithDebInfo 실행을 확인한다.
GPU와 화면 표시 경로가 다를 수 있으므로 Windows와 WSL의 fps 차이를 곧바로 엔진 회귀로 판단하지 않는다.

## 8. WSL에서 빌드와 실행

### 8.1 작업 디렉터리

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

### 8.2 Debug 빌드

아래는 L1~L4 구현 후 저장소 루트에서 실행한다. 앞서 설치한 SDK 환경이 활성화된 터미널을 사용한다.

```bash
cmake -S . -B build/linux-debug -G Ninja \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-14 \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++-14 \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DNXT_BUILD_SANDBOX=ON -DNXT_BUILD_TESTS=ON \
    -DNXT_LINUX_ENABLE_XCB=ON -DNXT_LINUX_ENABLE_WAYLAND=ON
cmake --build build/linux-debug --parallel
ctest --test-dir build/linux-debug --output-on-failure
```

최초 configure에서 doctest를 받기 위한 네트워크 접근이 필요할 수 있다.
Configure가 실패했으면 바로 build하지 말고 먼저 원인을 해결한다.
컴파일러·SDK를 교체하면 같은 캐시를 재사용하지 말고 새 빌드 디렉터리 또는 `cmake --fresh`로 다시 구성한다.

```bash
./build/linux-debug/apps/sandbox/sandbox \
    --window-system xcb --objects 1000 --culling on --log-level info
./build/linux-debug/apps/sandbox/sandbox \
    --window-system wayland --objects 1000 --culling on --log-level info
```

각 창을 닫은 뒤 다음 명령을 실행한다. 처음에는 작은 입력으로 기능을 확인하고 그다음 10만 오브젝트로 확장한다.

### 8.3 RelWithDebInfo 빌드

```bash
cmake -S . -B build/linux-bench -G Ninja \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-14 \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++-14 \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DNXT_BUILD_SANDBOX=ON -DNXT_BUILD_TESTS=OFF \
    -DNXT_LINUX_ENABLE_XCB=ON -DNXT_LINUX_ENABLE_WAYLAND=ON
cmake --build build/linux-bench --parallel
./build/linux-bench/apps/sandbox/sandbox \
    --window-system xcb --objects 100000 --culling on --present immediate --log-level info
```

Wayland는 `--window-system wayland`로 바꿔 같은 입력으로 실행한다.
요청한 present 방식이 미지원이면 실제 선택한 방식도 기록한다.
WSLg 표시 경로를 포함한 결과이므로 네이티브 Linux나 Windows의 기준선을 대체하지 않는다.

### 8.4 프리셋 추가

L5에서 `linux-debug`·`linux-bench` configure/build 프리셋과 Debug용 test 프리셋을 추가한다.
단일 구성 Ninja이므로 configure의 `CMAKE_BUILD_TYPE`을 반드시 지정한다.
Windows MSVC 프리셋과 빌드 출력 경로를 공유하지 않고 Linux GCC를 지정한다.
현재 이 프리셋은 미구현이므로 위의 직접 configure 절차를 구현 검증의 기준으로 사용한다.

## 9. WSL의 VS Code와 clangd

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
        "NXT_LINUX_ENABLE_XCB": "ON",
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
L5 이후에는 `cmake.useCMakePresets`를 `always`로 바꾸고 Linux configure 프리셋을 선택한다.
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

## 10. 완료 조건과 Phase 1 복귀

- [ ] WSL2·WSLg·GCC·CMake·Ninja·clangd·SDK와 실제 Vulkan 장치를 기록했다.
- [ ] Linux Debug 빌드와 공통 테스트가 성공한다.
- [ ] XCB와 Wayland 양쪽에서 같은 Sandbox를 표시한다.
- [ ] 강제 선택·auto·미포함·연결 실패 결과가 명확히 로그에 나온다.
- [ ] 연속 리사이즈, 최소화/복귀에 해당하는 조작, 닫기와 초기화 도중 실패를 검증했다.
- [ ] Debug validation 활성화를 확인했고 ERROR/VUID가 없다.
- [ ] RelWithDebInfo의 CPU/GPU/fence/draws가 나오며 timestamp 미지원을 구분한다.
- [ ] clangd가 Linux compile DB로 앱과 양쪽 백엔드를 분석한다.
- [ ] Windows MSVC 빌드·실행·테스트가 유지된다.
- [ ] 네이티브 Linux 미검증, fractional scaling 등 지원 제한을 기록했다.

L5에서 새 검증 기록을 만들고 실행 명령·결과·제한을 정리한다.
여기까지 통과하면 Phase 1-B2로 돌아간다. 포팅 중에 Job·Scheduler·Event의 기능 범위를 넓히지 않는다.

### 작업량 예상

WSLg와 Vulkan 드라이버를 사용할 수 있다는 전제에서 XCB 우선 표시까지 1~3일,
Wayland와 선택·회귀 검증을 포함해 전체 3~7작업일을 예상한다.
구현 경험과 환경에 따라 달라지는 추정이며 드라이버 구축·API 요구 변경이 필요하면 별도 산정한다.

## 11. 관련 자료

- [Phase 1](phase1.md) — 복귀할 CPU 런타임·이벤트 계획
- [RHI 설계](../design/rhi.md) — Vulkan을 격리하는 경계
- [프레임 설계](../design/frame.md) — 동기화와 자원 파괴
- [Microsoft WSLg](https://github.com/microsoft/wslg) — X11·Wayland 실행 환경
- [VS Code WSL](https://code.visualstudio.com/docs/remote/wsl) — Linux 측 확장·도구 실행
- [clangd 설정](https://clangd.llvm.org/config) — compile DB와 분석 설정
