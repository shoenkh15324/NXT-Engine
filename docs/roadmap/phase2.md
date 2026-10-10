# Phase 2 — 실제 데이터

> 목표: **디스크의 3D 파일을 읽어 화면에 띄운다.**
>
> 삼각형을 직접 만들어 칠하던 것을, 파일에서 읽은 메시를 그리는 것으로 바꾼다.

---

## 1. 왜 이제 파일인가

Phase 0과 1에서 우리는 삼각형을 직접 만들었다. 하지만 "삼각형 10만 개"는
실제 콘텐츠가 아니다. 실제 게임에서는 여러 종류의 애셋이 들어온다.

이 Phase에서 바뀌는 것은 데이터 흐름이다.

```text
Phase 0-1 : CPU에서 정점을 만든다 → GPU로 올린다
Phase 2   : 디스크에서 읽는다   → 파싱한다 → GPU로 올린다
```

파싱은 시간이 걸리고, 실패할 수 있고, 비동기적으로 처리할 수 있다.
**이것이 CPU 런타임에 job을 하나 더 주는 이유**이며, Phase 1에서 만든 큐를
실제로 쓸 수 있는 첫 사례가 된다.

또한 이 Phase가 **GPU 업로드 경로**를 처음 요구한다. 로컬 버퍼 → staging →
GPU 버퍼라는 경로는 지금 없으며, 지금 필요없으니 Phase 0에서 만들지 않았다.

## 2. 이미 결정된 것

### 2.1 `renderer/world` vs `engine/scene` — 확정

Unreal의 `UWorld → SceneProxy → FScene` 구조를 따른다.

```text
engine/scene     게임 로직 · 트리 · 컴포넌트 · 변환 — 논리적 소유
      │  SceneProxy — 등록 시 1회 복사, 단방향
      ↓
renderer/world   GPU 핸들 · 컬링 결과 · 그릴 대상 — 렌더링 소유
```

- `engine/scene`은 `nxt_graphics`를 모른다
- `renderer/world`는 `engine/scene`을 모른다
- `SceneProxy`는 `nxt_renderer` 안에 있고, 물체 단위로 만든다
- 매 프레임 임시 상태는 `FrameContext`에 있고, `World`만 지속된다

근거와 렌더 스레드 도입 기준은 [`../design/scene_world.md`](../design/scene_world.md)에 있다.

### 2.2 Asset 수명 — 핸들 기반, refcounting 없음

**핸들을 보관하고 포인터를 보관하지 않는다.**

```text
BAD   MeshPtr mesh_;      참조 카운트가 0이 되지 않는다
GOOD  MeshID meshId_;     필요할 때만 resolve
```

`nxt_core::handle::Handle<Tag>`를 쓴다. 이미 세대(generation)를 지원하므로
해제된 핸들을 구분할 수 있다.

매니저가 소유하며 두 단계 수명을 갖는다.

```text
SharedAssetManager   앱 전체 수명 — 공용 머티리얼, UI 폰트
LocalAssetManager    부모 씬 수명 — 레벨 메시, 레벨 텍스처
```

**refcounting은 넣지 않는다.** Phase 2는 매니저 소유 + 명시적 `unload()`다.
refcounting을 넣을 시점은 **같은 리소스를 서로 다른 씬이 공유할 때**이며,
그 요구가 실제로 생길 때 도입한다. 지금 넣으면 `CODING_RULES.md` §10이 금지하는
추상화가 된다.

**`assets`는 GPU 리소스를 만들지 않는다. `renderer/world`는 파일을 읽지 않는다.**
파싱된 데이터와 올라간 리소스가 서로를 모르게 하는 것이 수명 버그를 막는다.

## 3. 구현 파일

### 3.1 `nxt_assets` (신규 모듈)

```text
src/nxt/assets/
    asset_type.hpp
    asset_registry.hpp
    asset_registry.cpp
    asset_manager.hpp
    asset_manager.cpp
    loaders/
        asset_loader.hpp
        mesh_loader.hpp
        mesh_loader.cpp        cgltf 래핑
        texture_loader.hpp
        texture_loader.cpp
```

- `asset_type.hpp`는 애셋 종류 태그와 핸들만 정의한다. 로직은 없다.
- `Handle<MeshTag>`처럼 Phase 0의 `Handle`을 재사용한다.
- `AssetManager`는 읽기 요청을 받아 job으로 던지고 완료를 알린다.
  **로딩은 절대 렌더링 스레드에서 동기으로 하지 않는다.**

`assets`는 `platform`(파일 읽기)과 `graphics`(GPU 업로드)에 의존한다.
두 하위 계층에 의존하는 모듈이므로, 하위 모듈이 `assets`를 알면 안 된다.

### 3.1.1 glTF 파서 — cgltf

| 후보 | 판단 |
|---|---|
| **cgltf** | **채택.** 단일 C99 파일. Filament · bgfx · raylib 실사용 |
| tinygltf | C++11 + nlohmann/json + stb_image. 예외 억제 플래그가 필요하다 |
| fastgltf | C++20 네이티브로 가장 빠르지만 가장 어리다 |

채택 근거:

1. **순수 C라 예외가 구조적으로 불가능**하다. `CODING_RULES.md` §13이 예외 없는
   빌드를 목표로 하는데, 다른 둘은 플래그로 억누른다.
2. **단일 파일, 의존성 트리 0.** `external/`이 비어 있는 현재 구조에 맞는다.
3. **상용 엔진에서 검증됐다.** Filament이 cgltf를 쓴다.
4. cgltf는 버퍼를 직접 읽으라고 하는데, 이 제약이 NXT에 맞는다.
   `platform/filesystem`이 파일 I/O를 소유하고 `assets`가 비동기 로딩을 소유해야 하므로
   로더가 `FILE*`를 직접 여는 것은 경계를 위반한다.

`external/`에 넣지 말고 저장소에 직접 포함하거나 CMake `FetchContent`로 가져온다.

### 3.2 `nxt_graphics` — image, texture, staging

```text
src/nxt/graphics/
    rhi/
        image.hpp
        texture.hpp
        upload_buffer.hpp
```

Phase 0에서 제외했던 것들이 여기서 필요해진다.

```text
CPU 정점 배열
    ↓ memcpy
host-visible upload buffer     ← CPU가 직접 쓴다
    ↓ vkCmdCopyBuffer
GPU vertex buffer              ← GPU가 읽는다
```

`upload_buffer`는 이것을 한 번에 감추는 RHI 단위다. renderer는 memcpy 한 번만 한다.
Vulkan의 transient command buffer가 아니라 명시적 버퍼로 시작한다.
근거 없이 고난도 경로로 가지 않는다.

### 3.3 `nxt_renderer` — mesh와 material

```text
src/nxt/renderer/
    mesh/mesh.hpp
    mesh/mesh.cpp
    material/material.hpp
    material/material.cpp
```

`renderer/world`가 GPU mesh 핸들을 보관하고, `assets`는 CPU 측 데이터만 준다.
**파싱된 asset이 GPU 리소스를 직접 만들지 않는다.** 두 책임의 분리.

## 4. 마일스톤

### 2-A — 메시 파일

최소 glTF 스텝을 읽어 메시 하나로 그린다. 텍스처는 아직 없다.

**완료 기준**

- [ ] 파일 경로를 넘기면 읽어져 화면에 보인다
- [ ] 잘못된 파일을 주면 오류를 보고하고 죽지 않는다
- [ ] 로딩 시간이 측정된다
- [ ] 로딩이 렌더링 스레드를 막지 않는다

### 2-B — 텍스처

image/texture와 staging 업로드를 붙인다.

**완료 기준**

- [ ] 텍스처가 매핑된 메시가 보인다
- [ ] mipmap 생성이 동작한다
- [ ] GPU 업로드 비용이 측정된다

### 2-C — 여러 메시와 로딩

씬 단위로 여러 메시를 로드하고 전환한다.

**완료 기준**

- [ ] 씬을 전환할 수 있다
- [ ] 전환 중 GPU 사용 중인 리소스를 안전하게 처리한다
- [ ] 로딩 중에도 이전 씬이 계속 그려진다 (프레임에 정지하지 않는다)

## 5. 이 Phase의 실질적 내용

기능 목록이 아니다. 다음 셋을 확정하는 Phase다.

1. `world`와 `scene`의 경계
2. Asset 수명 정책 — 언제 언로드하고, GPU 사용 중이면 어떻게 하는가
3. CPU 파싱과 GPU 업로드의 분리 지점

## 6. 제외

- Render graph, multi-pass, depth, shadow → Phase 3
- PBR 머티리얼 → Phase 3+. 2-B까지는 단색 + 텍스처 샘플
- Asset 스트리밍, 비동기 디코딩, 압축 텍스처(KTX2) → Phase 4 이후
- Scene graph의 컴포넌트 시스템 → 경계 확정 후
- 에디터, 직렬화, 리소스 핫 리로드 → Phase 4 이후

## 7. 미해결

| 질문 | 결정 시점 |
|---|---|
| Asset 수명의 세부 — 언로드 정책, 월드 전환 중 GPU 사용 중인 리소스 | 2-C 착수 전 |
| glTF 지원 범위 — 전체를 읽을 것인가 최소만 읽을 것인가 | 2-A 착수 전 |
| 로딩 실패 시의 정책 — 부분 로드인가 전부 실패인가 | 2-A |
| refcounting 도입 여부 | 공유 요구가 실제로 생길 때 |
| 텍스처 압축(KTX2) | Phase 4 이후 |

## 8. Phase 2 완료 시점

```text
디스크 → assets → renderer/world → graphics → 화면
```

삼각형을 직접 만들지 않고도 장면을 띄울 수 있는 상태.
이후의 모든 작업은 실제 콘텐츠 위에서 하게 된다.