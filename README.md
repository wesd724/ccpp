# C/C++ Project Laboratory

이 저장소는 C와 C++로 만들 수 있는 프로그램을 분야와 규모의 제한 없이 축적하는 프로젝트 모음이다. 물리 시뮬레이션만을 위한 저장소가 아니다. 작은 콘솔 실험, Win32 GUI, 그래픽스와 게임, 수치 계산, 시스템 프로그래밍부터 장기적으로는 대규모 엔진과 복합 응용 프로그램까지 모두 다룬다.

현재 프로젝트는 Windows와 Cygwin을 중심으로 실행되지만, 저장소의 최종 범위는 특정 플랫폼이나 분야에 고정하지 않는다. 각 프로그램은 완성품이면서 동시에 언어, 수학, 컴퓨터과학, 그래픽스, 물리와 공학을 실제 코드로 검증하는 학습·연구 단위다.

## 저장소 원칙

- 프로젝트 하나마다 독립 폴더를 사용한다.
- 폴더 이름의 `-c`, `-cpp` 접미사로 구현 언어를 구분한다.
- 새 기능은 실제로 실행되는 코드뿐 아니라 목적, 이론, 기술 선택, 한계와 후속 목표까지 문서화한다.
- 작은 예제도 어떤 원리로 동작하는지 설명하고, 큰 프로젝트는 개요와 상세 문서를 계층적으로 분리한다.
- 구현과 문서가 다르면 코드를 현재 사실로 보고 문서를 같은 변경에서 바로 고친다.
- 최신 안정 언어 표준과 API를 우선하되, 현재 남아 있는 과도기·레거시 구조는 숨기지 않고 기술 부채로 기록한다.

세부 작업 규칙은 [AGENTS.md](AGENTS.md)에 있다.

## 현재 개발 환경

2026-07-12에 이 작업 공간에서 직접 확인한 환경이다. 모든 프로젝트의 영구적인 최소 요구 사항을 뜻하지는 않는다.

| 항목 | 현재 환경 |
|---|---|
| 운영체제·셸 | 64비트 Windows, PowerShell, Cygwin 도구 체인 |
| C/C++ 컴파일러 | GCC/G++ 14.4.0, `x86_64` |
| 빌드 도구 | GNU Make 4.4.1 |
| 네이티브 GUI | Win32 API, GDI |
| 실시간 그래픽스 | WGL, OpenGL 3.3/4.3/4.6 경로 |
| 실시간 오디오 | WASAPI `IAudioClient3`, 48 kHz stereo, 현재 최소 주기 128 frame |
| 병렬 계산 | OpenMP, OpenGL compute shader |
| 현재 검증 GPU | NVIDIA GeForce RTX 3060 Ti 8GB |

대부분의 프로젝트는 해당 폴더에서 다음 명령으로 빌드하고 실행한다.

```sh
make run
```

빌드만 하려면 `make`, 생성물을 지우려면 `make clean`을 사용한다. 프로젝트별 차이와 조작법은 각 `README.md`를 따른다.

저장소 루트에서는 다음 공통 명령을 제공한다. 루트 `make`는 열 개 프로젝트를 빌드하며 프로그램을 실행하지 않는다.

```sh
make                 # 전체 프로젝트 빌드
make check           # 장치 독립 회귀 테스트와 Conway 규칙 테스트
make check-graphics  # Windows OpenGL 드라이버의 실제 픽셀 출력 검사
make check-audio     # 기본 WASAPI 출력 장치 검사 (소리 재생 없음)
```

검증 범위와 한계는 [tests/README.md](tests/README.md), 최근 전체 점검 결과와 남은 과제는 [유지보수 점검](docs/MAINTENANCE.md)에 정리한다. 각 프로젝트 내부의 기존 `make run`은 그대로 사용한다.

## 프로젝트 카탈로그

난이도는 코드 길이가 아니라 이론, 수치 안정성, 플랫폼 결합, 렌더링과 병렬화까지 포함한 상대적인 구현 난이도다. `1`은 입문 규모, `5`는 여러 전문 영역을 함께 다루는 연구·엔진 규모다.

| 프로젝트 | 분야·형태 | 언어·표준 | 핵심 기술과 개념 | 난이도 | 현재 성격 |
|---|---|---|---|---:|---|
| [rotating-cube-c](rotating-cube-c/README.md) | 콘솔 ASCII 3D | C23 | 3축 회전, 원근 투영, 문자 프레임버퍼, Z-buffer, ANSI 제어 | 2/5 | 완결된 소형 학습 예제 |
| [conways-game-of-life-c](conways-game-of-life-c/README.md) | 콘솔 셀룰러 오토마타 | C23 | B3/S23, Moore 이웃, 동시 이중 버퍼, 토러스 경계, ANSI 편집 UI | 2/5 | 상호작용 가능한 생명 게임 실험기 |
| [rotating-cube-cpp](rotating-cube-cpp/README.md) | 콘솔 ASCII 3D | C++23 | C 버전과 같은 수학, RAII, `std::array`, 구조적 바인딩, signal-safe 종료 상태 | 2/5 | C/C++ 설계 비교 예제 |
| [keyboard-piano-cpp](keyboard-piano-cpp/README.md) | 콘솔 Piano·Drum 악기 | C++23 | WASAPI 저지연 출력, 다성·타악 합성, ADSR, noise filtering, stereo reverb | 3/5 | 외부 음원 없는 연주 가능한 신시사이저 |
| [double-pendulum-c](double-pendulum-c/README.md) | Win32/GDI 물리 GUI | C23 | 결합 비선형 ODE, RK4, 카오스, GDI 이중 버퍼링 | 3/5 | 상호작용 가능한 수치 시뮬레이션 |
| [fluid-sim-c](fluid-sim-c/README.md) | Win32/GDI 2D 유동 | C23 | Stable Fluids 계열, 이류·확산·압력 투영, 격자 경계 | 3/5 | CPU 유체 알고리즘 학습기 |
| [ray-tracer-c](ray-tracer-c/README.md) | CPU 오프라인 렌더러 | C23 | 광선 교차, 재귀 반사, hard shadow, Blinn-Phong, supersampling | 3/5 | 소형 Whitted-style 레이트레이서 |
| [opengl-shooter-c](opengl-shooter-c/README.md) | 2D OpenGL 게임 | C23 | 실시간 게임 루프, 객체 풀, 원 충돌, 조준·연사, 입자 효과 | 3/5 | 플레이 가능한 아케이드 프로토타입 |
| [opengl-shooter-3d-c](opengl-shooter-3d-c/README.md) | 3D OpenGL FPS | C23 | 1인칭 카메라, AABB 충돌, hitscan raycast, 대형 절차식 맵 | 4/5 | 단일 파일 3D 게임 프로토타입 |
| [fluid-sim-3d-c](fluid-sim-3d-c/README.md) | GPU 복합 물리 샌드박스 | C23 | 높이장, FLIP/PIC, APIC/MLS-MPM, 화면 공간 수면, OpenMP·compute | 5/5 | 계속 확장하는 주력 연구 프로젝트 |

표의 표준은 현재 각 `Makefile`에 지정된 실제 컴파일 모드다. 기존 C17/C++20 프로젝트도 현재 GCC/G++ 14.4.0이 지원하는 C23/C++23으로 전환했다. 컴파일 모드 지정이 표준의 모든 기능을 컴파일러가 완전히 구현한다는 뜻은 아니다.

## 프로젝트별 개요

### 콘솔과 3D 수학

#### rotating-cube-c

그래픽 API 없이 콘솔 문자 배열만으로 회전하는 입체를 만든다. 큐브 여섯 면을 점으로 샘플링하고 X·Y·Z 회전행렬을 순서대로 적용한 뒤 원근 나눗셈으로 80×40 화면에 투영한다. 픽셀별 역깊이 Z-buffer가 뒤쪽 면을 가린다. C 배열, 포인터 출력 매개변수, 신호 처리와 POSIX 시간 함수가 핵심이다.

#### rotating-cube-cpp

같은 시각 결과를 C++23의 값 타입과 자원 수명 관리 방식으로 다시 구성한 비교 프로젝트다. `Terminal`의 생성자·소멸자가 커서 표시 상태를 복구하고, `CubeRenderer`가 화면과 깊이 버퍼를 소유한다. 알고리즘 차이보다 C와 C++의 표현력·불변식·소유권 차이를 보는 프로젝트다.

### 셀룰러 오토마타

#### conways-game-of-life-c

콘웨이의 `B3/S23` 규칙을 유한 격자에서 동시에 갱신하고 ANSI 콘솔에 실시간 표시한다. Block, Blinker, Glider, Pulsar와 Gosper glider gun을 커서 위치에 배치할 수 있으며 셀 편집, 한 세대 진행, 1~60 generation/s 속도 조절과 죽은 외부·토러스 경계를 지원한다. 생명 규칙, 패턴과 크기 변경은 터미널 프런트엔드와 분리되어 자동 테스트된다.

### 오디오와 음악

#### keyboard-piano-cpp

컴퓨터 키보드의 17개 키를 반음 단위 건반으로 사용하거나 `Tab`으로 여덟 패드의 Drum Kit을 연주하며 최대 48개 voice를 동시에 합성한다. Windows 10 이상의 `IAudioClient3` 공유 모드에서 장치가 허용하는 최소 engine period를 요청하고 MMCSS `Pro Audio` 스레드가 event-driven buffer를 채운다. 네 Piano 음색의 ADSR·부분음 합성뿐 아니라 kick pitch sweep, snare·clap noise burst, hi-hat·crash 비정수 금속 공진과 choke를 직접 계산하며 FFmpeg나 외부 음원은 필요하지 않다.

### 수치 계산과 물리

#### double-pendulum-c

질량이 없는 두 막대와 두 점질량으로 구성된 평면 이중 진자의 결합 운동방정식을 4차 Runge-Kutta로 적분한다. 초기 조건에 민감한 카오스 운동과 수치 적분을 관찰할 수 있으며, 마지막 추의 900개 위치를 GDI 궤적으로 표시한다.

#### fluid-sim-c

160×100 격자의 속도장과 밀도장을 CPU에서 계산한다. 속도 확산, 압력 투영, semi-Lagrangian 이류, 밀도 확산·이류 순으로 진행하며 마우스 이동량을 운동량으로 주입한다. 자유 수면 물이 아니라 비압축성 2D 유동 속의 연기·잉크형 수동 스칼라를 표현한다.

#### fluid-sim-3d-c

현재 가장 큰 프로젝트다. 60 FPS 편집용 2.5D 높이장과 물체 물리, 3D FLIP/PIC 입자 모드, 화면 공간 연속 수면, GPU MPM 재료 실험실을 하나의 애플리케이션 안에서 독립 모드로 제공한다. 자세한 비교는 [모드 카탈로그](fluid-sim-3d-c/docs/MODES.md), 내부 데이터 흐름은 [아키텍처 문서](fluid-sim-3d-c/docs/ARCHITECTURE.md), 장기 연구 목표는 [시네마틱 로드맵](fluid-sim-3d-c/docs/CINEMATIC_TARGET.md)에 있다.

### 렌더링

#### ray-tracer-c

OpenGL 없이 CPU가 720×405 영상을 직접 계산한다. 구와 평면의 해석적 교차, 가장 가까운 hit 선택, 점광원 hard shadow, Blinn-Phong 직접광과 최대 깊이 4의 재귀 반사를 사용한다. 픽셀마다 고정된 2×2 표본을 평균하고 감마 변환한 뒤 GDI로 표시하거나 PPM으로 저장한다.

### 게임과 실시간 그래픽스

#### opengl-shooter-c

2D 화면 좌표에서 플레이어, 총알, 적과 파티클을 고정 크기 객체 풀로 관리한다. 마우스를 향한 정규화 방향벡터, 원-원 충돌, 점수에 따른 생성 주기와 연속 발사를 구현한다. 그래픽 경로는 공용 호환 레이어를 통해 GLSL 3.30과 동적 VBO로 변환된다.

#### opengl-shooter-3d-c

144×144 규모의 훈련장 안에 벽, 엄폐물, 기둥과 표적을 코드 데이터로 배치한다. 플레이어 이동은 원 대 AABB 충돌과 축별 sliding을 사용하며, 발사는 투사체가 아니라 카메라 광선과 맵 AABB·적 구의 가장 가까운 교차를 구하는 hitscan 방식이다. 현재 1,300줄이 넘는 단일 파일이므로 기능 확장 전에 모듈 분리가 중요한 개선 과제다.

## 지식과 기술 지도

| 영역 | 현재 다루는 내용 | 관련 프로젝트 |
|---|---|---|
| C·C++ 언어 | 구조체, 포인터, 동적 메모리, 콜백, RAII, STL 컨테이너, 원자 상태 | 전체, 특히 두 rotating cube |
| 이산수학·셀룰러 오토마타 | B3/S23, Moore 이웃, 동시 상태 전이, 정지물·진동자·우주선, 토러스 | conways-game-of-life-c |
| 운영체제·GUI | Win32 창 클래스, 메시지 루프, 입력, 타이머, GDI 자원 수명 | GUI 프로젝트 전체 |
| 실시간 오디오·DSP | WASAPI, PCM, 다성 voice, 12평균율, ADSR, additive·타악 합성, noise filter, reverb, limiter | keyboard-piano-cpp |
| 선형대수 | 벡터, 내적, 정규화, 회전·뷰·투영 행렬, 좌표계 변환 | cube, shooter, ray tracer, 3D fluid |
| 수치해석 | RK4, Gauss-Seidel/Jacobi 반복, semi-Lagrangian 이류, 고정 시간 간격 | pendulum, fluid 프로젝트 |
| 충돌·기하 | 원·구, AABB, ray-sphere, ray-plane, ray-AABB, 경계 투영 | shooter, ray tracer, MPM |
| 실시간 렌더링 | WGL, GLSL, VAO/VBO, depth, blend, framebuffer, 화면 공간 합성 | OpenGL 프로젝트 |
| 오프라인 렌더링 | 카메라 광선, 재귀 반사, shadow ray, supersampling, 감마 | ray-tracer-c |
| 유체역학 | 비압축성 투영, 높이장 파동, FLIP/PIC, 압력 Poisson 해법 | fluid 프로젝트 |
| 연속체·재료역학 | 변형 구배, polar decomposition, 탄성·소성 항복, MPM/APIC | fluid-sim-3d-c M 모드 |
| 병렬·GPU 계산 | OpenMP, SSBO, compute dispatch, 원자 연산, memory barrier | fluid-sim-3d-c |
| 게임 구조 | 입력 상태, delta time, 객체 풀, 생성 난이도, 점수·체력·재시작 | shooter 프로젝트 |

## 공용 파일과 비실행 자료

### modern_gl_compat.h

[modern_gl_compat.h](modern_gl_compat.h)는 두 슈팅 프로젝트가 사용하는 과도기 렌더링 호환 레이어다. 소스의 `glBegin`·`glVertex*` 형태 호출을 매크로로 가로채 CPU 배열에 모은 뒤, GLSL 3.30 프로그램과 VAO/VBO를 사용해 `glDrawArrays`로 출력한다.

따라서 드라이버의 고정 파이프라인만 사용하는 코드는 아니지만, 이상적인 현대 OpenGL 구조도 아니다. 행렬 변환이 CPU에서 수행되고 draw마다 VBO를 다시 업로드하며, 법선·조명·재질·fog 호환 함수 일부는 실제로 아무 효과가 없다. 새 프로젝트에서 이 API를 사용하지 않으며, 기존 슈팅 프로젝트도 외부 셰이더·영구 GPU 버퍼·명시적 uniform 기반 렌더러로 교체하는 것이 목표다.

### 참고 자료

물리 시뮬레이션의 참고 이미지·영상·논문은 관련 프로젝트 문서에서 관리한다. 개인적으로 수집한 `pic/` 이미지가 모든 checkout에 포함된다고 가정하지 않는다. 참고 자료는 구현 완료 증거가 아니며, 실제 지원 기능은 코드와 실행 검증을 기준으로 판단한다.

## 문서 구조

| 위치 | 책임 |
|---|---|
| 루트 `README.md` | 저장소 목적, 전체 프로젝트 탐색, 비교와 공통 기술 상태 |
| 각 프로젝트 `README.md` | 실행법, 현재 기능, 기술 스택, 이론·알고리즘, 파일, 한계와 로드맵 |
| 프로젝트 `docs/*.md` | 한 README로 관리하기 어려운 아키텍처, 모드, 논문과 장기 설계 |
| [CHANGELOG.md](CHANGELOG.md) | 확인된 사용자 기능, 구조와 주요 기술 변경의 날짜별 기록 |
| 루트·프로젝트 `AGENTS.md` | 저장소 공통 규칙과 특정 프로젝트에만 적용되는 추가 작업 규칙 |

문서는 소스 코드의 부록이 아니라 프로젝트의 일부다. 프로젝트 추가, 기능 변경, 알고리즘 교체, 키 변경, 성능 특성 변경, 의존성·빌드 변경, 파일 이동이 생기면 같은 작업에서 관련 문서를 갱신한다. 계획 기능과 구현 기능은 반드시 구분하고, 측정하지 않은 성능 수치는 추정 또는 목표라고 표시한다.

## 공통 개선 방향

1. 현재 C23/C++23 빌드를 유지하고 컴파일러·의존성 변경 시 전체 회귀 검증을 수행한다.
2. 두 슈팅 프로젝트에서 `modern_gl_compat.h` 의존성을 제거하고 네이티브 현대 OpenGL 렌더러로 모듈화한다.
3. 단일 파일 프로젝트가 커지면 플랫폼, 수학, 시뮬레이션, 렌더링과 입력 모듈을 분리한다.
4. 수치 프로젝트에 에너지·질량·발산 오차 같은 검증 지표와 재현 가능한 테스트를 추가한다.
5. 공통 `make check`에 더 많은 수치 오차 검사와 GUI 실행 smoke test를 추가한다.
6. 플랫폼 종속 Win32 계층과 핵심 알고리즘을 분리해 다른 창·입력 백엔드로 확장할 수 있게 한다.
7. 새 프로젝트는 시스템, 네트워크, 데이터베이스, 컴파일러, 오디오, 영상, 도구, 과학 계산, AI, 게임 엔진 등 현재 목록에 없는 분야도 자유롭게 추가한다.

## 공통 참고 자료

- [Microsoft: Windows messages and message queues](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
- [Microsoft: Low Latency Audio](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio)
- [Khronos OpenGL Registry](https://registry.khronos.org/OpenGL/index_gl.php)
- [OpenGL 4.6 core reference pages](https://registry.khronos.org/OpenGL-Refpages/gl4/html/start.html)
- [ECMA-48 terminal control functions](https://ecma-international.org/publications-and-standards/standards/ecma-48/)

프로젝트에 직접 대응하는 논문, 공식 문서와 학습 자료는 각 프로젝트 README에 따로 기록한다.
