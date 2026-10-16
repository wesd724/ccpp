# 변경 기록

이 문서는 저장소 전체에서 사용자에게 보이는 기능, 프로젝트 구조, 주요 알고리즘·렌더링 경로와 문서 체계의 의미 있는 변경을 기록한다. 사소한 문장 수정이나 formatting은 기록하지 않는다.

아직 Git에 남아 있지 않은 과거 이력은 추측해 작성하지 않는다. 아래 날짜 이후 실제로 확인된 변경만 누적한다.

## 2026-10-16

### 전체 점검과 검증

- 열 개 프로젝트를 점검하고 기존 C17/C++20 빌드를 C23/C++23으로 전환했다.
- 루트 `make`, `make check`, `make check-graphics`, `make check-audio`를 추가하고 Makefile·공용 GL 헤더 변경의 빌드 의존성을 보완했다.
- 2D 유체 브러시의 배열 범위 침범, resize 좌표, 반복되는 마우스 이동량 주입과 capture 손실을 수정했다.
- 이중 진자를 QPC 실측 accumulator로 진행해 기존 약 1.5배속을 바로잡고 RK4 에너지·시간 회귀 테스트를 추가했다.
- 레이트레이서의 좌우 반전·잘못된 재질 선택·표본 범위를 수정하고 8행 batch 렌더로 UI 응답과 저장 오류 처리를 개선했다.
- 두 shooter의 포커스 입력·발사 잔류, 종료 뒤 draw와 GL 자원 정리를 개선했다. 2D 조준은 resize를 반영하고 추적·감쇠는 시간 기준으로 보정했다.
- 3D 유체의 포커스·capture 손실 때 드래그 release를 전달하고 초기화 실패·종료 경계를 정리했다. 기존 네 모드와 GPU 물리·셰이더는 유지했다.
- 콘솔 큐브의 signal-safe 종료를 보완하고 오디오 형식 GUID·PCM valid bits와 종료된 스레드 재시작 처리를 보강했다.
- Conway의 기능키·수정키 방향키 입력과 크기 검증을 보강하고 규칙 테스트를 11개로 확대했다. 512개 국소 상태와 패턴 주기·셀 나이 포화를 포함한다.
- 여덟 공통 회귀 실행 파일, 실제 OpenGL 픽셀 검사를 추가하고 프로젝트 README, 테스트 설명과 전체 점검 문서를 갱신했다.

## 2026-07-17

### conways-game-of-life-c

- C23으로 작성한 상호작용형 ANSI 콘솔 콘웨이 생명 게임 프로젝트를 추가했다.
- `B3/S23` 규칙을 현재·다음 세대 이중 버퍼로 동시에 갱신하고 셀 나이, population, birth와 death 통계를 표시한다.
- 죽은 외부와 토러스 경계, 1~60 generation/s 속도, 일시정지·한 세대 진행, 커서 셀 편집과 터미널 크기 변경 시 중앙 보존을 구현했다.
- Block, Blinker, Glider, Pulsar와 Gosper glider gun 내장 패턴 및 밀도 24% 난수 초기화를 추가했다.
- 시뮬레이션, 패턴, 터미널과 실행 루프를 모듈로 분리하고 일곱 개 규칙·경계·재현성 회귀 테스트를 `make check`에 연결했다.

## 2026-07-12

### keyboard-piano-cpp

- C++23, Win32 keyboard polling과 WASAPI `IAudioClient3`를 사용하는 콘솔 피아노 프로젝트를 추가했다.
- 장치의 최소 공유 engine period를 사용하는 event-driven 오디오 스레드와 MMCSS `Pro Audio` 우선순위를 구성했다.
- 17개 반음 건반, 최대 48음 폴리포니, sustain, 옥타브·볼륨 조절과 네 가지 절차적 합성 음색을 구현했다.
- 악기별 envelope와 부분음 합성, stereo Schroeder 계열 reverb, soft limiter 및 float32·PCM 출력 변환을 추가했다.
- 기존 Piano를 유지하면서 `Tab`으로 전환하는 8-pad `Drum Kit` 모드를 추가하고 현재 모드와 입력을 콘솔 HUD에 표시했다.
- Kick·Tom pitch envelope, Snare resonant body와 filtered noise, 다중 burst Clap, 비정수 금속 공진 기반 Hi-hat·Crash를 one-shot voice로 합성한다.
- Piano sustain·release와 drum tail이 같은 48개 voice pool에서 겹치며, Closed/Open Hat 재타격에는 35ms choke를 적용한다.
- 짧은 Piano `Pressed` 상태가 이전 Drum Kit 문자열의 뒷부분을 남기던 콘솔 잔상을 줄 단위 ANSI 지우기로 수정했다.
- `make check`로 기본 오디오 endpoint와 실제 sample rate·period를 검증할 수 있게 했다.

### 저장소 문서 체계

- 저장소 목적, 개발 환경, 전체 프로젝트 카탈로그와 공통 기술 상태를 설명하는 루트 `README.md`를 만들었다.
- 8개 실행 프로젝트의 README를 실제 코드 기준으로 전면 확장했다.
- 각 프로젝트에 기술 스택, 핵심 이론·공식, 알고리즘, 데이터 흐름, 주요 파일, 한계, 난이도, 개선 로드맵과 참고 자료를 기록했다.
- 두 OpenGL shooter가 `modern_gl_compat.h` 과도기 레이어를 사용하며 법선·조명·재질 호환 호출 일부가 no-op이라는 실제 상태를 명시했다.
- `AGENTS.md`에 코드와 문서를 같은 변경에서 동기화하는 저장소 전역 규칙을 추가했다.
- `fluid-sim-3d-c` 전용 모드·아키텍처·검증 규칙은 프로젝트 내부 `AGENTS.md`로 분리해 루트에는 공통 규칙만 남겼다.
- 모든 로컬 Markdown 링크, 전체 프로젝트 build와 console/GUI smoke test를 검증했다.

### fluid-sim-3d-c

- 기존 Preview, Realtime FLIP, High Quality Surface를 보존하면서 `M` 키의 `MPM Material Lab` 모드를 추가했다.
- 301,011개 입자와 `80×72×68` GPU 격자에서 탄성체, 점토성 소성체와 과립체를 APIC/MLS-MPM 계열 근사로 계산한다.
- 두 회전 충돌체, `F` 충격, 왼쪽 drag 외력, `R` reset과 모드 HUD를 연결했다.
- RTX 3060 Ti, 1440×900에서 MPM 모드를 약 55~60 FPS, GPU 사용률 약 92%로 측정했다.
- 모드별 상태와 난이도는 `docs/MODES.md`, 모듈·물리·렌더 데이터 흐름은 새 `docs/ARCHITECTURE.md`, 장기 목표는 `docs/CINEMATIC_TARGET.md`로 분리했다.
