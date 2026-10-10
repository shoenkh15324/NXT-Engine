# NXT-Engine 문서

엔진 아키텍처 노트와 설계 문서를 이 디렉터리에 추가합니다.

## 코딩 규칙

- [CODING_RULES.md](CODING_RULES.md) — 전 계층 공통 코딩 규약과 계층별 규칙 (명명, 포맷팅, API, 동시성 등)

## 설계 결정사항

- [design/logging.md](design/logging.md) — 로깅 시스템
- [design/rhi.md](design/rhi.md) — 그래픽 RHI 경계와 추상화 높이
- [design/frame.md](design/frame.md) — 프레임 수명과 present 소유권
- [design/scene_world.md](design/scene_world.md) — Scene과 Render World 경계

## 로드맵

로드맵은 부품 목록이 아니라 **아키텍처를 검증하는 순서**로 쓴다. 각 항목은
"무엇이 보인다 / 무엇이 증명됐다"로 끝난다.

- [roadmap/phase0.md](roadmap/phase0.md) — 삼각형
- [roadmap/phase1.md](roadmap/phase1.md) — 10만 삼각형, CPU 런타임
- [roadmap/phase2.md](roadmap/phase2.md) — 실제 데이터 (glTF)