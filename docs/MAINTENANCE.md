# 전체 프로젝트 점검

## 2026-10-16

열 개 프로젝트의 빌드 설정, 입력·종료 처리, 주요 알고리즘 경계와 관련 문서를 점검했다. 기존 모드는 삭제하지 않았고 전체 렌더러나 물리 엔진을 교체하지도 않았다. 범위가 명확하고 검증 가능한 오류 수정과 공통 검증 경로 추가에 집중했다.

| 프로젝트 | 이번 개선 | 남은 주요 과제 |
|---|---|---|
| `rotating-cube-c` | C23, signal handler를 종료 플래그 변경으로 제한, `.exe` clean | 터미널 resize와 실제 시간 기반 회전 |
| `rotating-cube-cpp` | C++23, `sig_atomic_t` 종료 상태 | C 버전과 같은 조건의 수학·출력 비교 테스트 |
| `double-pendulum-c` | QPC accumulator로 실제 시간 진행, pause·reset 시간 초기화, Space 반복 억제 | 궤적 GDI pen 캐시와 화면 크기 적응 |
| `fluid-sim-c` | 가장자리 브러시 범위 침범 수정, tick 사이 이동량 단일 소비, resize·capture 취소, 할당 실패 정리 | 비정방 격자 계수·발산·질량 검증, 물리 시간 동기화 |
| `ray-tracer-c` | 카메라 좌우 반전, 표본 정규화, 구 표면의 잘못된 바닥 무늬 수정, 8행 batch로 UI 응답, PPM 쓰기 오류 검사 | 렌더 코어 모듈화, 멀티스레딩과 이미지 회귀 테스트 |
| `opengl-shooter-c` | 포커스·capture 입력 취소, background 갱신 중단, resize 조준, 시간 기준 추적·감쇠, 종료 후 draw 방지 | native modern GL, 종횡비 유지와 swept collision |
| `opengl-shooter-3d-c` | 포커스 키·발사 해제, background 갱신 중단, 안전한 GL 종료 | 단일 파일 모듈화, 실제 조명·재질과 AI 경로 탐색 |
| `fluid-sim-3d-c` | 포커스·capture 드래그 취소, 초기화 실패·종료 경계 정리 | GPU 수치 검증과 capability 분리, 자원 할당 실패 경로 보강 |
| `keyboard-piano-cpp` | 전체 GUID·buffer stride 검증, valid PCM bits 처리, 종료된 스레드의 `start` 오인 수정, 장치 독립 DSP 검사 | endpoint 자동 복구, 예외 경로의 완전한 RAII와 장시간 glitch 계측 |
| `conways-game-of-life-c` | CSI·application cursor 입력 개선, 표현 가능 크기 검증, 512 상태·패턴 주기·나이 포화 등 테스트 확대 | 작은 터미널 레이아웃, 패턴 파일과 희소 격자 |

공용 `modern_gl_compat.h`는 모든 필수 함수 포인터를 확인하고 shader/program 실패와 정상 종료에서 GPU 자원을 정리한다. 여전히 CPU 변환·매 draw 업로드·조명 no-op을 가진 과도기 레이어이며 완전한 현대 renderer로 바뀐 것은 아니다.

## 검증

- 현재 GCC/G++ 14.4.0에서 모든 실행 프로젝트를 C23/C++23 모드로 경고 활성화 전체 빌드했다.
- `make check`의 여덟 공통 테스트와 Conway 열한 개 테스트가 통과했다. 여섯 공통 회귀 테스트는 수정 전 코드에서 실제 실패를 확인했다.
- `make check-graphics`에서 RTX 3060 Ti, OpenGL 4.6, NVIDIA 616.92의 실제 픽셀 출력·해제·재초기화를 확인했다.
- `make check-audio`에서 WASAPI 48 kHz, 최소 128 frame 주기를 확인했다. 이 값은 전체 출력 지연 측정값이 아니다.
- Markdown의 로컬 파일 링크 96개를 검사했다. 웹 링크의 가용성과 문서 내부 anchor 전체까지 검증한 것은 아니다.
- 검증 범위와 이후 실행 명령은 [tests/README.md](../tests/README.md)에 있다. 과거 물리 시뮬레이션 FPS·VRAM 수치를 이번 작업에서 재측정한 것으로 해석하지 않는다.

tracked 바이너리와 일부 기존 build 산출물이 저장소에 남아 있다. 이번에는 원래 추적 상태를 임의로 바꾸지 않고 향후 생성물의 ignore 규칙만 추가했다. 빌드로 tracked 바이너리가 변경될 수 있다.
