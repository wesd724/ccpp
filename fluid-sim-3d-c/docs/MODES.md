# fluid-sim-3d-c 모드 카탈로그

이 문서는 현재 구현된 실행 모드와 앞으로 추가할 모드의 특징, 구현 방식, 난이도, 한계와 성능을 한곳에서 관리하는 기준 문서다.

- 현재 모드의 정확한 동작과 비교: 이 문서
- 모듈, 상태 소유권과 물리·렌더 데이터 흐름: [ARCHITECTURE.md](ARCHITECTURE.md)
- 참고 영상·논문과 최종 기술 로드맵: [CINEMATIC_TARGET.md](CINEMATIC_TARGET.md)
- 실행 방법과 사용자 조작: [README.md](../README.md)

## 난이도 기준

난이도는 기존 코드가 없는 상태에서 해당 모드를 안정적으로 구현하고 검증하는 상대적인 공학 난이도다.

| 단계 | 의미 |
|---:|---|
| 1 | 단순한 표시 또는 기존 기능의 작은 변형 |
| 2 | 독립적인 기능과 기본적인 상태 관리 필요 |
| 3 | 수치 계산, 렌더 패스 또는 상호작용 중 하나가 복잡함 |
| 4 | 병렬 물리, 다중 시스템 결합 또는 GPU 동기화가 필요함 |
| 5 | 고해상도 물리와 고급 렌더링을 함께 최적화하고 검증해야 함 |

난이도는 코드 분량과 같지 않다. 짧은 GPU 동기화 코드나 압력 해석기가 긴 UI 코드보다 더 어려울 수 있다.

## 모드 비교

| 모드 | 상태 | 전환 키 | 물리 모델 | 대표 규모 | 렌더링 | 목표 성능 | 난이도 |
|---|---|---|---|---:|---|---:|---:|
| `Preview` | 구현됨 | 기본, `G`/`H`/`M`으로 이탈 | 2.5D 비선형 높이장 + 물체 물리 | `256×256` | 메시 수면 + 물체 + 포말 | 60 FPS | 4/5 |
| `Realtime FLIP` | 구현됨 | `G` | 3D dense FLIP/PIC | 126,300 입자 | 입자 구 진단 화면 | 60 FPS | 4.5/5 |
| `High Quality Surface` | 구현됨 | `H` | 고해상도 3D dense FLIP/PIC | 834,104 입자 | 화면 공간 연속 수면 + 광학 | 20~30 FPS | 5/5 |
| `MPM Material Lab` | 구현됨 | `M` | GPU APIC/MLS-MPM 근사 | 301,011 입자 | 재료별 구 입자 + 회전 충돌체 | 55~60 FPS | 5/5 |
| `Cinematic Bake` | 계획됨 | 미정 | sparse FLIP/APIC + 캐시 | 100만~수백만 입자 | SDF/메시 + path tracing | 오프라인 | 5/5 이상 |

## 모드 전환

| 현재 모드 | `G` 입력 | `H` 입력 | `M` 입력 |
|---|---|---|---|
| `Preview` | `Realtime FLIP` | `High Quality Surface` | `MPM Material Lab` |
| `Realtime FLIP` | `Preview` | `High Quality Surface` | `MPM Material Lab` |
| `High Quality Surface` | `Realtime FLIP` | `Preview` | `MPM Material Lab` |
| `MPM Material Lab` | `Realtime FLIP` | `High Quality Surface` | `Preview` |

오른쪽 위 HUD가 현재 모드, 시뮬레이션 종류와 전환 목적지를 표시한다. 전환 상태는 [main.c](../main.c), HUD 문구는 [hud_renderer.c](../src/hud_renderer.c)에서 관리한다.

## 공통 상태 규칙

- 네 모드는 같은 데이터를 서로 다른 방식으로 그리는 것이 아니라 각각 독립적인 시뮬레이션 상태를 가진다.
- 비활성 모드는 계산을 멈추고, 다시 진입하면 이전 상태에서 계속 진행한다.
- `R`은 `Preview`, `Realtime FLIP`, 초기화된 `High Quality Surface`와 `MPM Material Lab` 상태를 모두 초기화한다.
- 왼쪽 드래그와 `F`는 현재 활성 모드에만 외력을 주며, MPM 모드에서는 물결 대신 재료 충격으로 해석한다.
- 각 GPU 모드로 전환하면 해당 장면용 카메라 위치로, `Preview`로 돌아오면 Preview 카메라 위치로 초기화한다.
- `High Quality Surface`와 `MPM Material Lab`은 처음 진입할 때만 대형 GPU 버퍼와 셰이더를 지연 생성한다.

## Preview

### 목적

물체를 배치하고 드래그하면서 파도, 부력, 침식, 천과 파괴 구조물을 60 FPS로 편집하는 게임형 물리 샌드박스다.

### 구현

- `256×256` 2.5D 높이장
- 비선형 천수 계열 수면 계산과 위치별 수평 유속
- 120Hz 고정 시간 간격과 렌더 프레임 사이 상태 보간
- OpenMP를 사용한 CPU 격자 병렬화
- 물보라·포말 입자와 수면 메시 렌더링
- 강체 부력·항력·회전, 모래성 침식, 천 제약, 유리·레고·뗏목 파괴 연결
- 물체가 받은 힘의 반작용을 다시 수면 파동으로 전달

### 어려운 부분

- 높은 파도에서도 발산하지 않는 수치 안정성과 시간 간격 관리
- OpenMP 병렬 루프의 데이터 경합 방지
- 수면 높이·유속과 서로 다른 물체 해석기의 양방향 결합
- 천, 침식, 파괴 제약과 강체 충돌을 같은 고정 시간 루프에서 안정적으로 처리

### 특이사항과 한계

- 현재 물체 팔레트를 완전히 지원하는 유일한 모드다.
- 넓은 수면의 파동에는 효율적이지만 한 수평 위치에 높이를 하나만 가질 수 있다.
- 파도가 앞으로 말리는 오버행, 분리된 물 덩어리, 물체 뒤로 감싸 흐르는 유동은 표현할 수 없다.
- `Preview`라는 이름은 화질이 낮다는 뜻보다 빠른 편집과 상호작용을 위한 근사 해석기라는 뜻이다.

### 주요 소스

- [fluid_sim.c](../src/fluid_sim.c): 높이장, 유속, 포말과 물보라
- [physics_world.c](../src/physics_world.c): 물체, 부력, 침식, 천과 파괴 제약
- [renderer.c](../src/renderer.c): Preview 렌더 패스
- `shaders/water.*`, `shaders/floor.*`, `shaders/spray.*`

## Realtime FLIP

### 목적

3차원 유체 코어를 60 FPS로 실행하면서 입자 분포와 장애물 충돌을 직접 확인하는 실시간 진단 모드다.

### 구현

- 126,300개 3D 입자
- `48×32×48` dense collocated 격자
- 정수 원자 연산 기반 trilinear P2G
- 중력·사용자 외력, divergence, 36회 Jacobi 압력 반복과 속도 투영
- 94% FLIP / 6% PIC 혼합 G2P
- 입자 SSBO 두 개를 번갈아 사용하는 ping-pong 갱신
- 높이가 다른 다섯 개 고정 장애물과 원기둥 근사 충돌
- 입자 SSBO를 복사하지 않고 정점 입력으로 재사용

### 어려운 부분

- 여러 입자가 같은 격자 셀에 기록할 때 필요한 GPU 원자 연산
- 비압축성을 만드는 3D 압력 해석과 경계 조건
- compute dispatch 사이 SSBO 메모리 배리어와 ping-pong 순서
- P2G와 G2P 사이 에너지 손실, 수치 잡음과 안정성의 균형
- 60 FPS 예산 안에서 입자 수, 격자 크기, 압력 반복 횟수를 조절하는 작업

### 특이사항과 한계

- 물리는 3차원이므로 오버행과 분리 입자를 만들 수 있다.
- 입자를 구로 직접 그리므로 연속된 물보다 입자 덩어리처럼 보이는 것이 정상이다.
- 현재 격자는 sparse MAC grid가 아닌 dense collocated 검증용 구조다.
- 고정 장애물만 GPU 충돌에 연결되어 있고 Preview 물체 팔레트와는 아직 결합되지 않았다.
- RTX 3060 Ti에서 60 FPS를 목표로 제한한 프로필이다.

### 주요 소스

- [gpu_fluid.c](../src/gpu_fluid.c): 품질 설정과 FLIP 실행 순서
- [gpu_fluid.h](../include/gpu_fluid.h): GPU 상태와 품질 프로필
- `shaders/flip_*.comp`: P2G, 압력, 투영과 G2P
- `shaders/flip_particle.*`: 입자 진단 렌더링

## High Quality Surface

### 목적

GPU 사용률과 프레임률을 교환해 더 많은 입자와 연속적인 수면, 반사·굴절·흡수·그림자를 실시간에 가까운 속도로 보여주는 품질 우선 모드다.

### 구현

- 834,104개 3D 입자
- `88×58×88` dense collocated 격자
- 80회 Jacobi 압력 반복과 더 작은 물리 서브스텝
- 입자 깊이와 광학 두께를 별도 framebuffer에 렌더링
- 양방향 bilateral 계열 깊이 블러
- 깊이 미분을 이용한 연속 수면 위치와 법선 복원
- 기존 장면 복사본을 이용한 굴절
- Fresnel 환경 반사와 태양광 반사
- Beer-Lambert 흡수와 깊은 물 산란색
- 고정 장애물의 해석적 그림자

### 어려운 부분

- 80만 개가 넘는 입자 물리와 여러 전체 화면 렌더 패스를 한 프레임에 실행
- 입자 사이 구멍은 메우면서 물체와 수면 경계는 보존하는 깊이 블러
- 깊이 잡음을 과도한 반사나 흰 윤곽으로 증폭하지 않는 법선 복원
- 기존 장면 깊이와 복원 수면의 가림 순서 처리
- GPU 99% 부하에서 시뮬레이션 안정성과 입력 응답성을 함께 유지
- 해상도 변경 시 여러 색상·깊이 texture와 framebuffer를 안전하게 재할당

### 특이사항과 한계

- 최초 `H` 입력 시 약 83만 입자 버퍼와 렌더 타깃을 생성하므로 짧게 멈출 수 있다.
- RTX 3060 Ti, 1440×900 측정값은 약 25~26 FPS와 GPU 사용률 99%다.
- 수면은 완전한 3D 메시가 아니라 현재 카메라에서 얻은 깊이로 복원하는 화면 공간 표면이다.
- 화면 밖이나 다른 물체 뒤의 표면 정보는 없으며 카메라 경계, 얇은 물막과 겹친 표면에서 왜곡이 생길 수 있다.
- 경로 추적, 기포, 독립 whitewater 해석과 오프라인 프레임 캐시는 아직 포함하지 않는다.
- 입자 수만 늘린 `Realtime FLIP`이 아니라 별도의 수면 광학 렌더 파이프라인을 가진다.

### 주요 소스

- [gpu_fluid.c](../src/gpu_fluid.c): 고품질 물리 프로필
- [gpu_surface.c](../src/gpu_surface.c): 깊이·두께·블러·수면 합성 패스
- [gpu_surface.h](../include/gpu_surface.h): 렌더 타깃과 프로그램 상태
- `shaders/fluid_splat.vert`, `shaders/fluid_depth.frag`
- `shaders/fluid_thickness.frag`, `shaders/fluid_depth_blur.frag`
- `shaders/fluid_surface.frag`: 굴절, 반사, 흡수, 조명과 그림자

## MPM Material Lab

### 목적

물만 다루던 기존 세 모드와 달리 탄성체, 소성체와 과립체가 같은 장면에서 압축되고 늘어나며 항복하고 흩어지는 과정을 보여주는 GPU 재료 역학 실험실이다. 현재 장면은 서로 반대로 회전하는 두 충돌체 사이로 세 재료가 떨어지며 거동 차이를 드러낸다.

### 구현

- 301,011개 재료 입자와 `80×72×68` dense 공유 격자
- 입자마다 위치·속도, 변형 구배 `F`, APIC affine 행렬 `C`, 변형률과 손상 상태 저장
- quadratic B-spline의 `3×3×3` 이웃에 정수 원자 연산으로 P2G
- 변형 구배의 반복 polar decomposition과 fixed-corotated 계열 응력 근사
- 격자 중력·감쇠·외력·경계 조건과 회전 충돌체 속도 조건
- G2P 속도 보간, APIC affine 복원, 변형 구배 갱신과 재료별 항복 투영
- 빨간 탄성체: 높은 전단 강성과 넓은 탄성 한계
- 주황 점토: 낮은 항복 한계를 넘는 전단 변형을 소성 투영
- 파란 과립체: 인장 압력을 제한하고 낮은 전단 한계와 높은 감쇠 적용
- 최대 360Hz 물리 서브스텝, 프레임당 최대 18회 compute 실행
- `F` 중앙 충격과 수평면 왼쪽 드래그에 의한 국소 외력
- 실제 깊이를 기록하는 구 입자, 재료별 조도·색·변형 강조와 바닥 투영 그림자

### 어려운 부분

- 수십만 입자가 같은 격자 셀에 동시에 질량과 운동량을 기록할 때 생기는 경쟁과 원자 연산 비용
- 입자마다 반복되는 3×3 polar decomposition의 수치 안정성과 GPU 연산량
- 서로 다른 강성·항복 한계를 같은 시간 간격에서 발산 없이 유지하는 파라미터 조정
- 빠르게 회전하는 해석적 충돌체에서 입자가 관통하지 않도록 격자와 입자 단계 양쪽의 접촉을 일치시키는 작업
- C 구조체와 GLSL `std430` 구조체의 정렬, compute 배리어와 렌더 정점 입력의 동기화

### 특이사항과 한계

- 논문의 완전한 다중 재료 접촉 해법을 복제한 것이 아니라, 한 속도장을 공유하는 실시간 MLS-MPM 계열 근사 구현이다.
- 서로 다른 재료가 같은 셀에 들어가면 운동량이 섞인다. 재료별 속도장과 명시적 Coulomb 마찰을 가진 multifield contact는 아직 없다.
- 점토와 과립체의 항복은 SVD 기반 Drucker-Prager·Cam-Clay 전체 모델이 아닌 polar decomposition 기반 근사 투영이다.
- 회전 충돌체는 미리 정한 운동을 따르며 재료가 가하는 반작용으로 감속하거나 움직이지 않는다.
- 재료 표면은 메시 재구성이 아니라 깊이를 가진 구 입자 렌더링이다. 연속 고무 표면, 내부 균열 메시와 미세 모래 먼지는 후속 과제다.
- 물의 비압축성 압력 투영은 사용하지 않으므로 이 모드는 FLIP 물 모드가 아니다.
- RTX 3060 Ti, 1440×900 정상 상태 측정값은 약 55~60 FPS, GPU 사용률 약 92%, GPU 메모리 약 483 MiB다.

### 참고 기반

- [A Hybrid Material Point Method for Frictional Contact with Diverse Materials](https://qig.github.io/Hybrid_MPM.pdf): 다양한 재료의 MPM 구성과 접촉을 향한 장기 기준
- [Conformation Constraints for Efficient Viscoelastic Fluid Simulation](https://doi.org/10.1145/3130800.3130854): 점탄성 유체 모드의 후속 참고
- [Narrow Band FLIP](https://visualcomputing.ist.ac.at/publications/2016/NarrowBandFLIP/): 고해상도 자유 수면의 입자·연산 절감 방향

### 주요 소스

- [gpu_mpm.c](../src/gpu_mpm.c): GPU 자원, 재료 초기화, 외력과 compute 실행 순서
- [gpu_mpm.h](../include/gpu_mpm.h): 입자 메모리 배치와 격자 품질 설정
- `shaders/mpm_p2g.comp`: 재료 응력과 APIC P2G
- `shaders/mpm_grid.comp`: 외력, 경계와 회전 충돌체
- `shaders/mpm_g2p.comp`: G2P, 변형 갱신과 재료별 항복 투영
- `shaders/mpm_particle.*`, `shaders/mpm_shadow.*`: 재료 입자와 접촉 그림자

## Cinematic Bake

### 상태

아직 구현되지 않은 최종 오프라인 모드다. 세부 기술과 참고 논문은 [CINEMATIC_TARGET.md](CINEMATIC_TARGET.md)에서 관리한다.

### 목표

- sparse FLIP/APIC 또는 동급의 고해상도 3D 해석
- 시뮬레이션 결과의 프레임별 디스크 캐시와 결정적 재생
- narrow-band SDF, anisotropic kernel과 marching cubes 수면 메시
- spray·foam·bubble 분류와 독립적인 whitewater 동역학
- 움직이는 강체 SDF와 양방향 유체 결합
- path tracing 또는 고품질 ray tracing 최종 프레임 출력

### 특이사항

- 실시간 FPS가 목표가 아니다.
- 계산 중에는 한 프레임에 수초 이상 걸릴 수 있고, 완성된 캐시를 30/60 FPS로 재생한다.
- `High Quality Surface`를 대체하기보다 편집 결과를 최종 출력하는 후속 단계다.

## 새 모드 추가 체크리스트

새 모드를 만들거나 기존 모드의 성격을 크게 변경할 때 다음 항목을 함께 갱신한다.

- 모드 이름, 상태, 진입·이탈 키와 전환 표
- 사용자 목적과 기존 모드와의 차이
- 독립 상태인지 기존 상태를 공유하거나 변환하는지
- 물리 모델, 격자·입자 규모, 시간 간격과 해법 반복 횟수
- 렌더링 패스, 광학 효과와 필요한 GPU 자원
- 지원하는 카메라·마우스·키보드·물체 상호작용
- 가장 어려운 구현 지점과 알려진 실패 조건
- 표현할 수 없는 현상과 화면 공간·근사 모델의 한계
- 목표 하드웨어의 실제 FPS, GPU 사용률과 해상도
- 관련 C 파일, 헤더와 셰이더 경로
- 오른쪽 위 HUD 문구와 전환 목적지
- [README.md](../README.md)의 사용자 조작
- [CINEMATIC_TARGET.md](CINEMATIC_TARGET.md)의 장기 로드맵과 충돌 여부

새 모드는 단순히 키만 추가한 것으로 완료하지 않는다. HUD, 이 문서, 초기화·해제, `R` 동작, 모드 전환과 최소 한 번의 실제 화면·성능 검증까지 완료되어야 한다.
