# fluid-sim-c

C와 Win32/GDI로 구현한 CPU 2D 격자 유동 시뮬레이션이다. Jos Stam의 Stable Fluids 계열 구조를 따라 속도장을 확산·이류하고 압력 투영으로 발산을 줄인 뒤, 밀도장을 연기나 잉크처럼 운반한다.

이 프로젝트의 화면은 파도치는 자유 수면 물이 아니다. 화면 전체를 채운 비압축성 유체의 2차원 속도장 안에서 색 밀도라는 수동 스칼라가 이동하는 모델이다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 계산유체역학 입문, 수치 PDE, Win32 GUI |
| 언어 | C23 |
| 플랫폼·API | Windows desktop, Win32, GDI |
| 격자 | 내부 160×100, 경계용 ghost cell 포함 162×102 |
| 상태 | 밀도 2개, X·Y 속도 성분 각 2개 |
| 선형 해법 | in-place Gauss-Seidel 계열 16회 반복 |
| 이류 | 역추적 semi-Lagrangian + bilinear interpolation |
| 출력 | 160×100 BGRA 픽셀을 창 크기로 `StretchDIBits` 확대 |
| 구현 난이도 | 이론 3/5, 구현 3/5 |
| 주요 파일 | [main.c](main.c), [Makefile](Makefile) |

## 빌드와 실행

```sh
make run
```

| 입력 | 동작 |
|---|---|
| 마우스 왼쪽 드래그 | 주변 7×7 영역에 밀도와 마우스 이동 방향 속도 주입 |
| `R` | 모든 밀도·속도 배열 초기화 |
| `Esc` | 종료 |

## 상태와 파라미터

```text
dt        = 0.10
diffusion = 0.00004
viscosity = 0.00002
solver iterations = 16
timer interval = 16ms
```

`dt`는 현실의 초와 직접 일치하는 물리 단위가 아니라 이 격자 시뮬레이션의 경험적 시간 간격이다. 16ms마다 `dt=0.10`을 한 번 진행하므로 물리적 단위 보정 없이 “실시간 0.10초”라고 해석하면 안 된다.

배열은 다음 역할을 가진다.

| 배열 | 의미 |
|---|---|
| `u`, `v` | 현재 수평·수직 속도장 |
| `u_prev`, `v_prev` | 이전 상태, 선형 해법 임시 버퍼, 압력·발산 임시 저장소 |
| `density` | 현재 보이는 염료·연기 밀도 |
| `density_prev` | 밀도 확산·이류 입력 버퍼 |
| `pixels` | GDI에 전달할 160×100 packed RGB 영상 |

## 한 단계의 계산 순서

1. 마우스가 눌렸다면 밀도와 속도를 주입한다.
2. 현재 속도를 이전 버퍼에 복사한다.
3. 점성에 의한 X·Y 속도 확산을 각각 푼다.
4. 압력 투영으로 속도장의 divergence를 줄인다.
5. 투영된 속도로 속도장 자신을 semi-Lagrangian 이류한다.
6. 다시 압력 투영한다.
7. 밀도장을 확산한다.
8. 현재 속도장으로 밀도를 이류한다.
9. 밀도와 속도를 조금 감쇠시킨다.
10. 밀도와 속력으로 색상을 만들고 GDI에 표시한다.

이 순서는 `step_simulation`에 그대로 나타난다.

## 핵심 이론

### 비압축성 속도장

연속 방정식의 비압축성 조건은 다음과 같다.

```text
div(u) = 0
```

코드는 현재 속도의 이산 divergence를 계산하고 Poisson 형태의 압력 방정식을 반복 해법으로 근사한 다음, 속도에서 압력 gradient를 빼 발산 없는 성분으로 투영한다.

```text
u_projected = u - grad(p)
```

16번 반복은 정확한 수렴 보장이 아니라 속도와 비용 사이의 고정 품질 설정이다. 반복 횟수를 늘리면 divergence가 더 줄 수 있지만 CPU 비용도 증가한다.

### 확산

점성과 밀도 확산은 implicit 형태의 선형 시스템으로 바꿔 `linear_solve`에서 이웃 네 셀을 반복 참조한다. explicit 확산보다 큰 시간 간격에서 안정적이지만 반복을 유한 횟수만 수행하므로 근사 오차가 남는다.

### Semi-Lagrangian 이류

각 격자 셀에서 속도를 따라 시간을 거꾸로 추적한다.

```text
source_position = current_position - dt * velocity
```

source 위치의 값을 주변 네 셀에서 bilinear interpolation해 현재 셀에 저장한다. 이 방식은 안정적이고 구현이 단순하지만 수치 확산이 커서 소용돌이와 날카로운 밀도 경계가 빠르게 흐려진다.

### 경계 조건

배열 바깥에 한 셀 두께 ghost cell을 둔다. 벽에 수직인 속도 성분은 부호를 반전하고, 스칼라와 접선 성분은 가까운 내부 값을 복사한다. 네 모서리는 두 인접 경계 값의 평균이다.

## 상호작용과 표시

마우스 위치를 격자 좌표로 바꾸고 반경 약 4.5셀의 원형 falloff를 적용한다. 마우스 이동량 `dx`, `dy`는 속도에, 고정 세기는 밀도에 더한다. 따라서 클릭만 유지하면 색이 계속 추가되고, 빠르게 끌수록 강한 유동이 생긴다.

좌표 변환은 현재 client 크기를 사용한다. 가장자리에서는 7×7 브러시를 내부 격자에 잘라 적용해 ghost cell이나 배열 밖에 쓰지 않는다. 이동량은 마지막 물리 tick 이후의 누적 변위를 한 번만 소비하므로 마우스를 멈췄을 때 같은 속도를 반복 주입하지 않는다. 포커스나 capture를 잃으면 드래그를 취소한다.

표시 색은 물리적 스펙트럼이 아니다. 밀도를 보라·파랑 계열로 매핑하고 속력이 큰 곳에 빨강·초록 성분을 더한 뒤 간단한 vignette를 곱한 시각화다.

## Win32 구조

- `WM_CREATE`: 16ms timer 시작
- `WM_TIMER`: 시뮬레이션과 픽셀 갱신 후 paint 요청
- mouse messages: 위치와 드래그 상태 저장
- `WM_PAINT`: `StretchDIBits`로 CPU 픽셀 배열 표시
- `WM_DESTROY`: timer 종료와 message loop 탈출

별도 렌더링 라이브러리나 GPU 계산은 사용하지 않는다.

## 현재 한계

- 자유 수면, 파도 높이, 물방울과 3D 운동을 표현하지 않는다.
- collocated 2D 격자와 단순 반복 투영이라 전문 CFD 정확도를 목표로 하지 않는다.
- semi-Lagrangian 이류의 수치 확산으로 작은 와류가 빠르게 사라진다.
- 압력 residual, divergence norm, 질량 보존 오차를 측정하지 않는다.
- timer와 고정 `dt`가 현실 시간과 동기화되지 않는다.
- 비정방형 격자의 방향별 확산·투영 계수에 대한 수치 정확도 검증은 아직 별도 과제다.
- UI thread에서 모든 계산을 수행하며 OpenMP/SIMD 최적화가 없다.
- 장애물, 온도, 부력, vorticity confinement와 경계 편집이 없다.

## 개선 로드맵

1. 비정방형 격자의 방향별 격자 간격을 일관되게 적용하는 수치 모델 검증
2. fixed-step accumulator와 일시정지·single-step 기능
3. divergence와 밀도 총량 측정 및 테스트
4. MacCormack/BFECC 이류 또는 vorticity confinement로 세부 보존
5. 내부 장애물과 유입·유출 경계
6. 온도·부력 기반 연기와 여러 색 염료
7. OpenMP/SIMD 또는 compute shader 백엔드 비교
8. 3D 자유 수면 문제는 [fluid-sim-3d-c](../fluid-sim-3d-c/README.md)와 역할을 분리해 발전

## 참고 자료

저장소 루트의 `make check`는 모서리·창 바깥 드래그의 메모리 guard, ghost cell 보존, 이동량 소비, resize와 capture 취소를 검사한다. [공통 테스트](../tests/README.md)를 참고한다.

- [Jos Stam, Stable Fluids](https://graphics.stanford.edu/courses/cs448-01-spring/papers/stam.pdf)
- [Microsoft: StretchDIBits](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits)
- [Microsoft: Windows messages and message queues](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
