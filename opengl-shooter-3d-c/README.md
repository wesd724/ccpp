# opengl-shooter-3d-c

C, Win32/WGL과 OpenGL로 만든 1인칭 3D 슈팅 프로토타입이다. 144×144 규모의 실내 훈련장, 마우스 잠금 카메라, 벽 sliding, 추적형 적, hitscan 사격, tracer·impact particle, 간단한 총기 overlay와 접촉 그림자를 구현한다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 3D 게임, 실시간 그래픽스, 충돌·광선 질의 |
| 언어 | C23 |
| 플랫폼 | Windows desktop, Win32, WGL |
| 초기 창 크기 | 1100×760, resize 반영 |
| 맵 범위 | X·Z 각각 `[-72, 72]` |
| 렌더 경로 | `modern_gl_compat.h` 과도기 GLSL 3.30 + VAO/VBO |
| 사격 | ray-sphere/ray-AABB 기반 hitscan |
| 이동 충돌 | XZ 원 대 AABB·기둥, 축별 sliding |
| 구현 난이도 | 4/5 |
| 주요 파일 | [main.c](main.c), [Makefile](Makefile), [modern_gl_compat.h](../modern_gl_compat.h) |

## 빌드와 실행

```sh
make run
```

| 입력 | 동작 |
|---|---|
| `WASD` 또는 방향키 | 바라보는 수평 방향 기준 이동 |
| `Shift` | 빠른 이동 |
| 마우스 | yaw·pitch 시점 회전 |
| 왼쪽 버튼 또는 `Space` 유지 | 0.075초 간격 연속 hitscan 발사 |
| `R` | 위치·시점·점수·체력·적 초기화 |
| `Esc` | 종료 |

창에 focus가 생기면 커서를 숨기고 중앙에 고정한다. focus를 잃거나 game over가 되면 mouse capture를 해제하고 커서를 다시 표시한다.

포커스를 잃으면 키·발사 상태도 초기화하고 background에서는 게임 갱신과 전역 키 polling을 중단한다. 따라서 다른 창에서 타이핑한 키가 이동·발사로 적용되거나 Alt+Tab 뒤 이동이 붙는 현상을 방지한다. 종료 시 GL program/VAO/VBO를 context보다 먼저 해제하고 파괴된 창에 추가 프레임을 제출하지 않는다.

## 게임 상태

| 종류 | 최대 개수 | 역할 |
|---|---:|---|
| tracer | 48 | 발사 시작점에서 hit 지점까지 0.065초 동안 보이는 선 |
| 적 | 70 | 맵 가장자리에서 생성되어 플레이어를 추적 |
| particle | 520 | 총구·충돌·적 제거·플레이어 피격 효과 |

플레이어 체력은 5이며 적 하나를 제거할 때 10점을 얻는다. 점수가 높아질수록 적 속도가 조금 증가하고 spawn 간격이 짧아진다. 상태는 고정 배열의 `active` flag로 관리하므로 runtime allocation은 없다.

## 카메라와 좌표계

플레이어 위치가 카메라 위치이며 초기값은 `(0, 1.65, 50)`이다. yaw와 pitch로 forward를 만든다.

```text
forward = normalize(-sin(yaw) cos(pitch), sin(pitch), -cos(yaw) cos(pitch))
```

이동에는 Y성분을 제거한 `flat_forward`, `flat_right`를 사용한다. pitch는 약 ±71.6도로 제한해 카메라가 완전히 뒤집히지 않게 한다. 수직 FOV는 68도, near/far plane은 0.08과 220이다.

## 맵 구조

맵은 외부 model asset이 아니라 C 배열과 반복문으로 구성된다.

- 네 외곽 벽과 상단 경계
- 크기·높이·색이 다른 `MapBlock` 24개
- 5×5 규칙 격자의 얇은 기둥 25개
- 중앙 도로와 격자 바닥 표시
- 사격 lane, target board, 시작 구역 barrier
- 천장 light strip 형태의 밝은 면

렌더링 geometry와 충돌 geometry는 대부분 같은 `map_blocks` 데이터를 사용한다. 기둥은 별도 규칙 격자를 렌더·충돌·raycast에서 동일하게 반복 생성한다.

## 플레이어 이동과 충돌

플레이어의 수평 footprint를 반지름 `0.68`인 원으로 본다. 각 block의 가장 가까운 XZ점을 구해 원 중심과의 거리로 충돌을 판정한다.

```text
closest = clamp(circle_center, box_min, box_max)
collision if length(circle_center - closest) < radius
```

이동할 때 X축 후보를 먼저 검사해 허용되면 적용하고, 갱신된 위치에서 Z축 후보를 따로 검사한다. 한 축이 막혀도 다른 축으로 진행할 수 있어 벽을 따라 미끄러지는 sliding 효과가 생긴다. 완전한 continuous collision detection은 아니며 Y축 점프·경사·계단은 없다.

## Hitscan 사격

발사는 실제 탄환 강체를 시간에 따라 적분하지 않는다.

1. 카메라 forward와 총구 근사 위치로 ray를 만든다.
2. 외곽 벽, 24개 block과 25개 기둥의 ray-AABB 교차를 검사해 가장 가까운 맵 거리를 얻는다.
3. 그 거리보다 가까운 활성 적의 ray-sphere 교차를 검사한다.
4. 적이 먼저 맞으면 점수·파티클을 발생시키고 적을 제거한다.
5. 맵이 먼저 맞으면 impact particle을 만든다.
6. 시작점과 최종 hit 위치를 짧은 tracer line으로 표시한다.

### Ray-AABB slab 방법

각 축에서 ray가 box의 두 평면을 지나는 `t0`, `t1` 구간을 구하고 세 축 구간의 교집합을 취한다. 구간이 비면 miss다. 방향 성분이 거의 0이면 origin이 해당 축 slab 안에 있는지 직접 검사한다.

### Ray-sphere

`|origin + t direction - center|² = radius²`의 판별식을 계산하고 가장 가까운 양의 근을 사용한다.

이 raycast는 게임 판정을 위한 기하 질의다. [ray-tracer-c](../ray-tracer-c/README.md)처럼 모든 픽셀에 광선을 쏘아 영상을 만드는 ray tracing 렌더러가 아니다.

## 적 행동

적은 외곽 네 방향 중 하나의 무작위 위치를 최대 40번 시도한다. 장애물과 겹치지 않고 플레이어에서 16 이상 떨어진 위치만 선택한다. 생성 후 플레이어까지의 수평 방향을 매 프레임 정규화하고 같은 `move_with_collision`으로 이동한다.

navigation mesh나 pathfinding이 없으므로 큰 장애물 뒤에서 최단 경로를 찾지 못하고 벽을 향해 계속 밀 수 있다.

## 렌더링 구성

- 바닥·벽·엄폐물·적·파티클: box와 triangle geometry
- target·도로·light strip: 색상 면
- 적 접촉 그림자: 바닥의 반투명 타원 triangle fan
- tracer: depth test를 잠시 끈 line
- 총기·crosshair·muzzle flash: 3D 장면 뒤에 그리는 2D overlay
- depth buffer 24bit, alpha blending, back-face culling, double buffering

창 resize 시 `WM_SIZE`에서 client width와 height를 갱신해 viewport, aspect ratio와 weapon overlay를 다시 계산한다.

### OpenGL 현대화 상태

소스의 `glBegin`, matrix stack과 고정 파이프라인 조명 형태 호출은 공용 [modern_gl_compat.h](../modern_gl_compat.h)가 가로챈다. geometry는 CPU에서 clip-space로 변환되고 GLSL 3.30 shader·VAO·동적 VBO로 `glDrawArrays`된다.

중요한 현재 한계는 호환 레이어가 `glNormal3f`, `glLightfv`, `glMaterial*`, `glFog*`를 no-op으로 구현한다는 점이다. 따라서 코드에 조명 설정이 있어도 현재 shader는 vertex color만 보간하며 실제 per-pixel 또는 per-vertex lighting을 계산하지 않는다. 과거 README의 “현대 OpenGL 사용”은 draw 제출 방식만 부분적으로 맞았고, 렌더링 아키텍처 전체를 설명하기에는 부족했다.

## 메인 루프

```text
PeekMessage로 입력 처리
-> QueryPerformanceCounter delta time
-> game update
-> scene render
-> SwapBuffers
-> Sleep(1)
```

`dt`는 최대 0.05초로 제한한다. physics와 rendering이 같은 variable-step loop에 있어 frame rate에 따른 작은 결과 차이가 생길 수 있다.

## 주요 코드 위치

| 함수·자료 | 책임 |
|---|---|
| `map_blocks` | 절차식 맵의 block 자료 |
| `collides_map` | 외벽·block·기둥 충돌 |
| `move_with_collision` | 축별 이동과 sliding |
| `ray_hits_aabb`, `ray_hits_sphere` | hitscan 기하 질의 |
| `ray_map_distance` | 맵에서 가장 가까운 ray hit |
| `spawn_bullet` | hitscan 판정, 점수와 tracer 생성 |
| `update_game` | 입력, 적 생성·추적, 파티클과 체력 |
| `draw_map` | 훈련장 geometry 생성 |
| `draw_weapon` | crosshair·총기·muzzle flash overlay |
| `window_proc` | focus, mouse lock, resize와 key messages |

## 현재 한계

- 1,300줄이 넘는 단일 `main.c`에 플랫폼, 게임, 수학과 렌더링이 결합되어 있다.
- 임시 OpenGL 호환 레이어 때문에 법선·조명·재질 설정이 실제 shading에 반영되지 않는다.
- texture, normal map, shadow map, PBR, post-processing과 model asset이 없다.
- 적은 box 조합이고 skeletal animation·hit region이 없다.
- pathfinding, line-of-sight AI와 장애물 회피가 없다.
- 플레이어와 적 충돌은 2D footprint 근사이며 점프·경사·동적 강체가 없다.
- shot마다 모든 맵 primitive와 모든 적을 선형 검사한다.
- fixed physics step, replay, save, audio, menu와 상세 HUD가 없다.
- random seed가 실행 시간이라 자동 회귀 재현이 어렵다.

## 개선 로드맵

1. Win32 platform, input, math, world, collision, gameplay와 renderer 모듈 분리
2. OpenGL 4.x core context와 외부 GLSL, mesh VAO/VBO, uniform buffer 기반 native renderer
3. 실제 normal·directional/point light, shadow map과 HDR tone mapping
4. texture·material·model loader와 GPU instancing
5. broad-phase spatial grid/BVH와 swept movement
6. navigation mesh 또는 grid A*, 상태 머신과 다양한 적
7. projectile·weapon spread·recoil·reload와 audio
8. fixed tick, deterministic random seed, replay와 automated gameplay test
9. 맵 데이터를 코드에서 외부 scene format으로 분리

## 참고 자료

루트 `make check`에서 포커스 손실 뒤 키·발사 해제를 검사한다. 공용 GL 레이어의 함수 로딩과 compile/link 실패 처리를 보강했고 `make check-graphics`로 실제 픽셀 출력·해제·재초기화를 검사한다. 플레이 전체의 자동 검증은 아니며 [공통 테스트](../tests/README.md)에 범위를 기록한다.

- [Khronos OpenGL Registry](https://registry.khronos.org/OpenGL/index_gl.php)
- [OpenGL 4.6 core reference pages](https://registry.khronos.org/OpenGL-Refpages/gl4/html/start.html)
- [Microsoft: Windows messages and message queues](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
