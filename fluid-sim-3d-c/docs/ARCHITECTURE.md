# fluid-sim-3d-c 아키텍처

이 문서는 `fluid-sim-3d-c`의 모듈 경계, 상태 소유권, 수치 계산과 렌더링 데이터 흐름을 설명한다. 사용법은 [README.md](../README.md), 모드별 규모·난이도·한계는 [MODES.md](MODES.md), 장기 연구 방향은 [CINEMATIC_TARGET.md](CINEMATIC_TARGET.md)를 참고한다.

## 시스템 개요

애플리케이션 하나가 네 개의 독립 시뮬레이션 모드를 가진다.

```text
Win32/WGL platform
       |
       +-- input + camera
       |
       +-- Preview ---------------- CPU heightfield + PhysicsWorld
       |
       +-- Realtime FLIP ---------- GPU particle/grid state
       |
       +-- High Quality Surface --- independent high-density FLIP + GpuSurface
       |
       +-- MPM Material Lab ------- GPU material particle/grid state
       |
       +-- Renderer + HUD --------- active mode only
```

네 모드는 같은 물리 상태를 다른 방법으로 그리는 render mode가 아니다. 각각 별도 상태와 reset 함수를 가지며, 비활성 상태에서는 계산을 멈춘다. 다시 진입하면 `R`을 누르지 않은 한 이전 상태에서 계속 진행한다.

## 디렉터리 역할

| 위치 | 책임 |
|---|---|
| `main.c` | 모드 상태 기계, 입력 edge 검출, 시간 관리, update/render 조정 |
| `include/` | 모듈 공개 타입과 함수, compile-time 품질 상수 |
| `src/platform.c` | Win32 창, WGL context, 입력, VSync, buffer swap |
| `src/gl_api.c` | OpenGL 함수 pointer loading, shader 파일 compile/link |
| `src/camera.c` | 자유 카메라, view/projection, screen ray와 평면 교차 |
| `src/math3d.c` | 벡터와 4×4 행렬 연산 |
| `src/fluid_sim.c` | Preview 2.5D 높이장·속도·foam·spray |
| `src/physics_world.c` | Preview 물체, 부력·항력·침식·천·파괴 제약 |
| `src/gpu_fluid.c` | 두 FLIP 품질 profile과 compute dispatch 순서 |
| `src/gpu_surface.c` | High Quality 입자 깊이·두께·blur·수면 합성 |
| `src/gpu_mpm.c` | 재료 입자 생성, MPM compute 순서, 외력과 입자 렌더 |
| `src/renderer.c` | 모드별 scene pass와 renderer 조정 |
| `src/object_renderer.c` | Preview 강체·천·팔레트·배치 preview |
| `src/hud_renderer.c` | 현재 모드와 전환 키를 GDI texture로 생성·합성 |
| `shaders/` | GLSL 3.30 raster shader와 GLSL 4.30 compute/surface shader |
| `docs/` | 모드 카탈로그, 아키텍처와 장기 연구 목표 |

## 초기화와 종료

### 시작 순서

1. `platform_init`이 Win32 창과 pixel format을 만든다.
2. 임시 WGL context에서 `wglCreateContextAttribsARB`를 읽는다.
3. OpenGL 4.6, 4.5, 4.3, 3.3 core context를 순서대로 시도한다.
4. `gl_api_load`가 Windows `opengl32`에 직접 없는 함수 pointer를 읽는다.
5. CPU `FluidSim`, 공용 `Renderer`, `Camera`, `PhysicsWorld`를 초기화한다.
6. Realtime `GpuFluid`를 즉시 초기화한다.
7. High Quality FLIP와 MPM은 최초 `H`, `M` 입력까지 지연 초기화한다.

Compute shader는 OpenGL 4.3 이상이 필요하다. context 생성에는 3.3 fallback 시도가 남아 있지만 `gl_api_load`가 compute 함수를 시작 시 필수로 읽으므로 현재 애플리케이션은 사실상 OpenGL 4.3 이상이 필요하다. 3.3 장치에서 Preview만 실행하는 기능 분리는 아직 구현하지 않았다. 현재 검증 환경은 OpenGL 4.6이다.

### 종료 순서

Renderer와 초기화된 GPU 모드의 program, buffer, texture, framebuffer, VAO를 삭제하고 CPU 배열을 해제한 뒤 WGL context와 Win32 창을 정리한다. 지연 초기화에 실패한 모드는 `available` flag로 destroy 대상에서 제외한다.

## 메인 루프

한 프레임의 상위 순서는 다음과 같다.

```text
platform_poll
-> high-resolution delta time 계산과 상한 적용
-> G/H/M/F/R edge-trigger 처리
-> camera_update
-> active mode interaction
-> active mode simulation step
-> active mode renderer
-> mode HUD
-> SwapBuffers
```

`G`, `H`, `M`, `F`, `R`은 이전 프레임 key state와 비교해 눌린 순간 한 번만 동작한다. 이동 키와 마우스는 유지 상태로 처리한다.

`WM_KILLFOCUS`에서 키와 마우스 유지 상태·카메라 delta를 지우고 capture를 해제한다. 포커스 또는 capture를 잃기 직전 왼쪽 버튼이 눌려 있었다면 `mouse_released`도 전달해 `PhysicsWorld`가 드래그를 종료하도록 한다. 초기 platform 생성 실패도 `platform_shutdown`으로 정리하며 종료 요청을 받은 poll 뒤에는 추가 update/render를 하지 않는다. 입력 취소 경로는 루트 `make check`에서 hidden Win32 window로 검증한다.

### 시간 모델

- 전체 frame delta는 큰 정지 후 폭주를 막기 위해 최대 0.08초로 제한한다.
- Preview 물리와 물체는 120Hz 고정 step accumulator를 사용한다.
- GPU FLIP는 profile별 최대 substep으로 frame delta를 나눈다.
- MPM은 최대 `1/360s`, 프레임당 최대 18 substep을 사용한다.
- rendering은 매 display frame 한 번 실행한다.

## Preview: CPU 높이장과 물체

### 상태

`FluidSim`은 `257×257` vertex 위치에 다음 필드를 가진다.

```text
eta(x,z)       surface displacement
u(x,z), v(x,z) horizontal velocity
foam(x,z)      breaking-wave visualization state
```

현재·다음 height와 두 velocity component를 ping-pong 배열로 소유한다. 최대 7,000개 spray particle은 별도 배열에 저장한다.

### 높이장 수치 모델

전체 수심을 `h = base_depth + eta`로 본다. 한 step에서 속도를 반-Lagrangian 방식으로 역추적한 뒤 수면 gradient, 점성 Laplacian과 감쇠를 적용한다.

```text
u_next = advect(u) - g grad(eta) dt + viscosity laplacian(u) dt
```

수면 높이는 깊이를 곱한 수평 flux의 divergence로 갱신한다.

```text
eta_next = eta - div(h u_next) dt
```

이 구조는 비선형 천수방정식에서 영감을 받은 실시간 근사다. 전문 해안공학 solver의 conservative Riemann flux나 wet/dry front 해법은 아니다. 높이는 안정성을 위해 범위를 clamp하고 속도와 수면에 경험적 감쇠를 적용한다.

### 병렬화

셀별 속도와 높이 계산, vertex normal 생성은 OpenMP `parallel for`로 나눈다. 한 pass가 읽는 배열과 쓰는 배열을 분리해 데이터 경합을 피하고, pass 사이 implicit barrier 뒤 pointer를 교환한다.

### Breaking wave와 spray

수면 slope, flux compression과 속력으로 `breaking` 값을 만든다. 이 값이 높으면 foam field를 올리고 확률적으로 spray particle을 방출한다. spray는 중력과 수명으로 갱신한다. 이는 공기 혼입을 푸는 별도 two-phase solver가 아니라 시각 효과용 휴리스틱이다.

### PhysicsWorld 결합

`PhysicsWorld`는 최대 384개 body와 4개 cloth를 가진다. cloth 하나는 `14×10` 질점이다.

지원 prefab은 crate, sandcastle, cloth, glass, lego, raft다. 물체는 `fluid_sim_sample`로 위치의 수면 높이와 속도를 읽고 부력, 수평 항력과 회전 반응을 계산한다. 반작용은 높이장 disturb로 되돌려 양방향 결합을 근사한다.

파괴는 범용 FEM fracture가 아니라 body group과 breakable constraint의 integrity 규칙이다. 모래 침식도 다상 입자 해석이 아니라 잠김 시간과 물 흐름에 따른 결합력 감소 모델이다.

## GPU FLIP/PIC

Realtime와 High Quality는 같은 `GpuFluid` 구현에 서로 다른 품질 profile을 사용한다.

| 항목 | Realtime | High Quality |
|---|---:|---:|
| 입자 | 126,300 | 834,104 |
| 격자 | 48×32×48 | 88×58×88 |
| 압력 반복 | 36 | 80 |
| 최대 substep | 1/240s | 1/300s |
| 입자 간격 | 0.16 | 0.085 |

### GPU 자원

- 입자 SSBO 두 개: G2P 결과 ping-pong
- 정수 운동량·질량 accumulation grid
- 현재·이전 grid velocity
- divergence buffer
- pressure SSBO 두 개: Jacobi ping-pong
- particle VAO: 현재 입자 SSBO를 복사 없이 vertex source로 재사용

### Compute 순서

```text
clear grid
-> P2G atomic accumulation
-> normalize grid mass/momentum
-> gravity + impulses + obstacle boundary
-> divergence
-> Jacobi pressure iterations
-> velocity projection
-> G2P FLIP/PIC blend + particle advection
```

P2G는 여러 입자가 같은 cell에 쓰므로 정수 원자 연산으로 momentum과 weight를 누적한다. 정수 scale을 사용하는 것은 float atomic extension 의존성을 줄이기 위한 선택이며 양자화 오차와 overflow 범위를 관리해야 한다.

### 압력 투영

비압축성 조건을 만들기 위해 grid velocity의 divergence에서 압력 Poisson 방정식을 구성하고 Jacobi iteration으로 근사한다.

```text
laplacian(p) = density / dt * div(u)
u_next = u - dt / density * grad(p)
```

현재 구조는 dense collocated grid다. pressure residual 기준 조기 종료, staggered MAC grid와 multigrid/PCG는 아직 없다.

### FLIP/PIC 혼합

G2P에서 다음을 계산한다.

```text
v_pic  = sample(new_grid_velocity)
v_flip = particle_velocity + v_pic - sample(old_grid_velocity)
v_new  = mix(v_pic, v_flip, 0.94)
```

94% FLIP은 세부와 에너지를 더 보존하고, 6% PIC은 고주파 잡음을 줄인다.

## High Quality 화면 공간 수면

High Quality 물리는 FLIP 입자를 계산하지만 최종 화면은 입자 구를 그대로 보여주지 않는다. `GpuSurface`가 다음 pass를 실행한다.

1. 이미 그린 배경 scene color를 texture로 복사한다.
2. 입자를 point sphere로 rasterize해 camera-space depth를 `R32F`에 기록한다.
3. additive blending으로 광학 두께를 별도 texture에 누적한다.
4. depth를 horizontal·vertical bilateral 계열 blur한다.
5. fullscreen pass가 depth 미분으로 위치와 normal을 복원한다.
6. 배경 texture 굴절, Fresnel 환경 반사, Beer-Lambert 흡수, 태양광과 장애물 그림자를 합성한다.

화면 공간 방식은 현재 카메라에서 빠르게 연속 표면을 만들지만 화면 밖, 뒤쪽, 여러 겹 표면 정보가 없다. 완전한 3D SDF/mesh 재구성이 아니다.

## MPM Material Lab

### 입자 상태

MPM 입자 하나는 다음 데이터를 `std430` 정렬에 맞춘 vec4 단위로 저장한다.

- 위치와 material id
- 속도와 표시용 strain
- 변형 구배 `F`의 세 column
- APIC affine velocity `C`의 세 column
- Jacobian, damage, 색 variation과 active state

현재 301,011개 입자와 `80×72×68` grid를 사용한다.

### Compute 순서

```text
clear grid
-> particle stress + APIC P2G
-> grid gravity, impulse, rotor contact, boundaries
-> G2P velocity + affine reconstruction
-> F update + material projection
-> particle/rotor/floor collision
```

quadratic B-spline weight로 각 입자를 `3×3×3` 이웃 cell에 전달한다. 변형 구배에서 반복 polar decomposition으로 회전 `R`을 근사하고 fixed-corotated 계열 Kirchhoff stress를 구성한다.

```text
J = det(F)
tau = 2 mu (F - R) F^T + lambda J (J - 1) I
F_next = (I + dt C) F
```

실제 shader는 재료에 따라 체적항과 clamp를 달리하며 수치 안정성을 위한 범위 제한을 포함한다.

### 재료 모델

- Elastic: 높은 `mu`, `lambda`, 넓은 shear yield와 낮은 damage 증가
- Clay: 낮은 yield를 넘는 shear를 빠르게 투영해 영구 변형 근사
- Granular: 인장 volumetric stress 제한, 매우 낮은 shear yield와 큰 감쇠

현재는 공유 grid velocity 하나를 사용하므로 서로 다른 재료가 같은 cell에서 momentum을 섞는다. multifield MPM의 재료별 속도장, Coulomb frictional contact, SVD 기반 Drucker-Prager/Cam-Clay 전체 구성식은 아니다.

### 충돌체와 입력

두 5-arm rotor의 위치와 각속도는 analytic하게 정해진다. grid stage와 particle stage가 같은 시간·형상을 사용해 관통을 줄인다. rotor는 prescribed motion이므로 입자 반작용으로 감속하지 않는다.

- `F`: 두 개의 ellipsoidal downward impulse
- 왼쪽 drag: 화면 ray와 `y=1.15` 평면의 교차를 따라 국소 가속도 impulse

## 렌더링 계층

### Preview

- sky fullscreen triangle
- floor
- heightfield water indexed mesh
- object bodies와 cloth
- spray particles
- object palette와 selection UI

Water shader는 mesh normal, foam, view vector와 light 방향을 사용해 반사·흡수 계열 색을 근사한다. path tracing이나 실제 파장별 optics는 아니다.

### GPU particle mode

Realtime FLIP은 입자를 depth가 있는 point sphere로 직접 그려 grid/particle 상태를 진단한다.

### Material Lab

별도 sky와 stage shader, particle shadow pass, analytic rotor body, 재료별 point sphere를 그린다. 재료별 색·조도와 strain/damage를 시각화하지만 연속 표면 mesh는 만들지 않는다.

### HUD

모드가 바뀔 때 GDI `CreateDIBSection`에 title·detail·전환 key를 그려 OpenGL RGBA texture로 올린다. 매 프레임 text rasterization하지 않고 모드 변경 때만 texture를 갱신한다.

## OpenGL 상태와 동기화

- raster shader: GLSL 3.30 core
- compute와 high-quality surface shader: GLSL 4.30 core
- SSBO compute pass 사이: `GL_SHADER_STORAGE_BARRIER_BIT`
- compute particle 결과를 vertex로 읽기 전: `GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT`
- surface pass: framebuffer와 texture binding을 명시적으로 전환

CPU에서 GPU buffer를 매 프레임 readback하지 않는다. 물리 SSBO를 직접 vertex input으로 사용해 PCIe 복사를 피한다.

## 상태 소유권과 의존 방향

| 소유자 | 장기 상태 | 다른 모듈과의 관계 |
|---|---|---|
| `Platform` | HWND, HDC, HGLRC, InputState | 모든 입력·window size 제공 |
| `Camera` | position, yaw, pitch, FOV | renderer와 ray interaction이 읽음 |
| `FluidSim` | CPU 높이·속도·foam·spray | PhysicsWorld와 Preview Renderer가 사용 |
| `PhysicsWorld` | body·cloth·selection | FluidSim을 sample/disturb, ObjectRenderer가 읽음 |
| `GpuFluid` | 입자·grid·pressure SSBO | active GPU mode에서 update/draw |
| `GpuSurface` | depth·thickness·blur FBO | High Quality `GpuFluid` 내부에서만 사용 |
| `GpuMpm` | material particle·grid SSBO | Material renderer가 시간·particle draw 사용 |
| `Renderer` | program, mesh, object/HUD renderer | active state를 읽지만 물리를 수정하지 않음 |

공개 header가 소유 타입과 API를 정의하고 구현 `.c`가 자원 생명주기를 책임진다. header 이름과 같은 `.c`를 include로 찾는 구조가 아니며, [Makefile](../Makefile)이 모든 `src/*.c`를 각각 compile한 뒤 link한다.

## 빌드 구조

```text
main.c + src/*.c
  -> 개별 build/*.o
  -> dependency build/*.d
  -> fluid_sim_3d.exe
```

`-MMD -MP`가 project header dependency를 생성한다. GLSL은 실행 시 상대 경로 `shaders/...`에서 읽으므로 실행 working directory는 프로젝트 루트여야 한다. `make run`이 이 조건을 만족한다.

## 성능 기준

현재 RTX 3060 Ti, 1440×900 측정은 다음과 같다.

| 모드 | 측정값 |
|---|---|
| Preview | 60 FPS 목표, VSync 기준 |
| Realtime FLIP | 60 FPS |
| High Quality Surface | 약 24~26 FPS, GPU 약 99% |
| MPM Material Lab | 과거 약 55~60 FPS, GPU 약 92%; 483 MiB 메모리 기록은 프로세스별 계측 근거가 없어 앱 전용값으로 확정하지 않음 |

측정값은 driver, 창 크기, VSync와 동시에 초기화된 다른 GPU 모드에 따라 달라질 수 있다.

## 알려진 구조적 한계

- 네 해석기가 공통 scene graph나 공통 collision SDF를 공유하지 않는다.
- Preview 물체 palette가 GPU FLIP/MPM과 직접 결합되지 않는다.
- FLIP은 dense collocated grid, MPM은 dense shared velocity grid다.
- High Quality 표면은 screen-space reconstruction이다.
- shader hot reload, GPU timestamp profiler와 automated image regression이 없다.
- renderer와 일부 simulation profile이 runtime capability query보다 compile-time 상수에 의존한다.
- OpenGL context가 낮은 버전으로 fallback했을 때 기능별 사용자 오류 설명이 충분하지 않다.

## 확장 규칙

새 모드를 추가할 때는 [MODES.md의 체크리스트](MODES.md#새-모드-추가-체크리스트)를 따른다. 코드 구조상 최소 요구 사항은 다음과 같다.

1. 소유 상태와 init/reset/step/draw/destroy API 정의
2. lazy init 여부와 실패 시 fallback 결정
3. `main.c` AppMode, edge-trigger key, camera reset과 render branch 연결
4. HUD 현재 모드·전환 목적지 갱신
5. README, MODES, 이 문서의 데이터 흐름 갱신
6. clean build, 실제 화면, 입력, reset, mode round-trip과 성능 측정

## 연구 참고

- [Jos Stam, Stable Fluids](https://graphics.stanford.edu/courses/cs448-01-spring/papers/stam.pdf)
- [Narrow Band FLIP](https://visualcomputing.ist.ac.at/publications/2016/NarrowBandFLIP/)
- [A Hybrid Material Point Method for Frictional Contact with Diverse Materials](https://qig.github.io/Hybrid_MPM.pdf)
- [Khronos OpenGL Registry](https://registry.khronos.org/OpenGL/index_gl.php)
