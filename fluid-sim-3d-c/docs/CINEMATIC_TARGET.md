# fluid-sim-3d-c 시네마틱 목표

이 문서는 참고 영상·논문과 최종 `Cinematic Bake` 기술 로드맵을 관리한다. 현재 및 앞으로 추가되는 실행 모드의 특징, 구현, 난이도와 성능 비교는 [MODES.md](MODES.md), 모듈과 데이터 흐름은 [ARCHITECTURE.md](ARCHITECTURE.md)에서 관리한다.

## 목표 영상

- Two Minute Papers, `Finally, A Blazing Fast Fluid Simulator!`
  - https://www.youtube.com/watch?v=i4KWiq3guRU
  - 원 논문: `Fast Fluid Simulations with Sparse Volumes on the GPU`
  - https://people.csail.mit.edu/kuiwu/gvdb_sim.html
- Two Minute Papers, `Is a Realistic Water Bubble Simulation Possible?`
  - https://www.youtube.com/watch?v=O-52enqUSNw
  - 원 논문: `Unified Spray, Foam and Bubbles for Particle-Based Fluids`
  - https://cg.informatik.uni-freiburg.de/publications/2012_CGI_sprayFoamBubbles.pdf

## 현재 구현의 위치

기본 `fluid_sim.c`는 2.5D 높이장 기반 비선형 천수 방정식 해석기다. 넓은 수면의 파동, 부력과 구조물 상호작용을 60 FPS로 편집하기에는 적합하지만 다음 현상을 표현할 수 없다.

- 파도가 앞으로 말리고 끊어지는 오버행
- 공중에서 분리되는 물 덩어리와 얇은 물막
- 물체를 완전히 감싸고 뒤쪽으로 흐르는 3차원 유동
- 체적 보존 압력과 3차원 와류
- 물속 기포, 표면 거품, 미세 비말
- 모래·탄성체·유체가 같은 입자/격자에서 접촉하는 다중 재료 역학

따라서 현재 해석기는 `Preview` 모드로 유지하고, 목표 화질은 별도의 `Cinematic` 파이프라인으로 구현한다.

## 최종 파이프라인

### 1. 3D 유체 코어

- FLIP/APIC 입자와 3D MAC 격자
- 입자-격자 P2G, 외력, 비압축성 압력 투영, G2P
- GPU matrix-free PCG 이후 geometric multigrid 압력 해법
- CFL 기반 적응 시간 간격과 서브스텝
- 유체량 보존과 입자 재배치
- 움직이는 물체의 SDF 경계와 양방향 힘 전달

### 2. 희소 도메인

- 활성 유체 주변 brick만 할당하는 sparse brick pool
- 빈 공간 연산과 VRAM 사용 제거
- 8GB VRAM 기준 Preview 10만~50만 입자, Cinematic 100만~400만 입자 목표
- 시뮬레이션 프레임을 디스크 캐시에 저장하고 60 FPS로 재생

### 3. 다중 재료

- 물: FLIP/APIC
- 모래·눈·점토: MLS-MPM 또는 APIC-MPM과 재료별 항복 모델
- 천·고무: XPBD/FEM
- 유리·레고·구조체: 강체와 파괴 가능한 제약
- 각 해석기는 공통 SDF/접촉 레이어와 힘을 교환

### 4. 자유 표면과 미세 효과

- 입자에서 narrow-band level set/SDF 생성
- anisotropic particle kernel로 표면 평활화
- GPU marching cubes 또는 dual contouring
- 곡률, 상대 속도, 운동 에너지로 whitewater 방출
- spray, foam, bubble 상태 분류와 서로 다른 이류·부력·수명
- 얇은 물막과 고립 물방울 보존

### 5. 렌더링

- 선형 HDR 파이프라인과 물리 단위 조명
- 물의 Fresnel, Beer-Lambert 흡수, 굴절, 분산 가능한 IOR
- 환경 반사, 태양광, 접촉 그림자, 수중 체적 산란과 caustics
- TAA, motion vectors, temporal denoise, bloom은 보조 효과로만 사용
- Preview: raster/compute 기반 실시간 렌더
- Cinematic: 프레임 캐시를 path tracing 또는 고품질 ray tracing으로 렌더

## 구현 순서

1. OpenGL 4.6 compute 기반 3D dense FLIP 수직 슬라이스
2. 입자 캐시와 결정적 재생
3. SDF 표면 재구성과 screen-space/PBR 물 렌더링
4. spray·foam·bubble 후처리
5. 움직이는 강체 SDF와 양방향 결합
6. sparse brick 도메인과 GPU 압력 해법 최적화
7. MPM 모래·탄성 재료 결합: 단일 공유 격자 실험실 구현됨, multifield 접촉과 정교한 구성 모델은 후속
8. 시네마틱 프레임 렌더와 내보내기

## 현재 구현 상태

두 가지 3D GPU 유체 프로필, 화면 공간 연속 수면과 GPU MPM 재료 실험실이 구현되어 있다.

- `G`: 126,300개 입자, `48×32×48` 격자, 36회 Jacobi 압력 반복의 60 FPS 디버그 모드
- `H`: 834,104개 입자, `88×58×88` 격자, 80회 Jacobi 압력 반복의 고품질 모드
- `M`: 301,011개 입자, `80×72×68` 격자에서 탄성체·점토·과립체를 계산하는 APIC/MLS-MPM 근사 모드
- 정수 원자 연산 기반 trilinear P2G
- 중력, 3D divergence, 품질별 Jacobi 압력 반복과 속도 투영
- 94% FLIP / 6% PIC 혼합 G2P
- 높이가 다른 다섯 개 장애물과 입자 충돌
- 시뮬레이션 SSBO를 복사 없이 렌더링 VBO로 재사용
- 화면 공간 입자 깊이·두께, 양방향 블러와 연속 수면 normal 복원
- 실제 배경 장면 굴절, Fresnel 환경 반사, Beer-Lambert 흡수, 태양광과 구조물 그림자
- MPM 변형 구배·APIC affine 상태, fixed-corotated 계열 응력, 재료별 항복 투영과 회전 충돌체
- RTX 3060 Ti 1440×900 측정값: `G` 60 FPS, `H` 약 24~26 FPS와 GPU 사용률 99%, `M` 약 55~60 FPS와 GPU 사용률 약 92%

`M`은 재료별 거동을 처음 연결한 실시간 기반이다. 한 격자 속도장을 공유하므로 논문의 multifield frictional contact나 SVD 기반 Drucker-Prager·Cam-Clay 모델 전체를 아직 구현하지 않는다. 다음 재료 단계는 재료별 속도장, Coulomb 접촉, 정교한 구성 모델, 움직이는 강체와의 양방향 힘 교환이다.

`H`의 화면 공간 표면은 카메라에서 연속적인 물을 실시간으로 보여주지만 완전한 3D 메시나 경로 추적 결과는 아니다. 다음 유체 구현은 narrow-band SDF 생성, anisotropic 입자 표면, marching cubes, whitewater와 프레임 캐시 순서로 진행한다. 현재 dense collocated 격자는 알고리즘 검증용이며 최종 단계에서는 sparse MAC grid와 matrix-free PCG/multigrid로 교체한다.

## 현재 하드웨어 기준

- GPU: NVIDIA GeForce RTX 3060 Ti, 8GB, compute capability 8.6
- 현재 드라이버: 516.94
- OpenGL: 4.6
- CUDA Toolkit: 11.2
- Visual Studio 2022 C++ 컴파일러 설치됨
- Vulkan SDK, DXC, Blender는 설치되지 않음

OpenGL, DirectX 12, Vulkan 중 어느 API를 쓰는지는 최종 화질을 결정하지 않는다. 현재는 추가 SDK 없이 compute shader와 렌더링을 함께 쓸 수 있는 OpenGL 4.6으로 3D 해석기를 시작한다. sparse CUDA/GVDB 또는 DXR 백엔드는 툴체인을 최신화한 뒤 선택적으로 추가한다.

## 성능 기대치

참조 논문의 GPU sparse FLIP도 수백만 입자 장면에서 약 5~7 FPS이며, 2,900만 입자 장면은 논문 장비에서 프레임당 약 1.8초가 걸린다. 논문 렌더는 1080p 128 spp에서 프레임당 약 10~30초다. 따라서 목표는 다음 네 단계를 함께 제공하는 것이다.

- `Preview`: 편집과 드래그 드롭을 위한 60 FPS 근사 물리
- `Realtime FLIP`: 60 FPS 3D 물리와 입자 진단
- `High Quality Surface`: GPU를 최대한 사용해 20~30 FPS로 상호작용하는 연속 수면
- `MPM Material Lab`: 프레임률보다 재료 다양성과 접촉 장면을 우선하는 GPU 입자·격자 실험실
- `Cinematic Bake`: 느리게 계산·저장한 뒤 고품질로 재생하고 path tracing하는 최종 3D 물리
