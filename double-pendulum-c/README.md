# double-pendulum-c

C, Win32 API와 GDI로 만든 평면 이중 진자 시뮬레이션이다. 두 각도와 각속도로 구성된 결합 비선형 운동방정식을 4차 Runge-Kutta 방법으로 적분하고, 두 번째 추의 궤적을 화면에 누적해 초기 조건에 민감한 카오스 운동을 관찰한다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 고전역학, 비선형 동역학, 수치 ODE, Win32 GUI |
| 언어 | C23 |
| 플랫폼·API | Windows desktop, Win32 message loop, GDI |
| 물리 상태 | `theta1`, `theta2`, `omega1`, `omega2` |
| 수치 적분 | 고정 간격 RK4, 내부 `dt = 0.006` |
| 표시 주기 | `WM_TIMER` 16ms, QPC 실측 시간에 따른 고정 물리 substep |
| 궤적 | 두 번째 추의 최근 900개 화면 위치 |
| 외부 라이브러리 | 없음, `gdi32`, `user32`, `libm` |
| 구현 난이도 | 이론 3/5, GUI 2/5 |
| 주요 파일 | [main.c](main.c), [Makefile](Makefile) |

## 빌드와 실행

```sh
make run
```

| 입력 | 동작 |
|---|---|
| `Space` | 일시정지·재개 |
| `R` | 초기 각도와 궤적 초기화 |
| `Esc` | 종료 |

## 물리 모델

두 막대는 질량이 없고 길이가 일정하며, 두 추는 점질량이라고 가정한다. 마찰, 공기 저항과 구동력은 없다.

```text
m1 = m2 = 1 kg
L1 = L2 = 1 m
g  = 9.81 m/s^2
theta1(0) = 1.65 rad
theta2(0) = 2.35 rad
omega1(0) = omega2(0) = 0
```

각도 0은 아래쪽 수직 방향이다. 첫 번째 추와 두 번째 추의 화면 위치는 다음 운동학을 사용한다.

```text
x1 = L1 sin(theta1)
y1 = L1 cos(theta1)
x2 = x1 + L2 sin(theta2)
y2 = y1 + L2 cos(theta2)
```

코드의 `derivatives`는 상태 `y = (theta1, theta2, omega1, omega2)`에 대해 다음 1계 ODE를 반환한다.

```text
dy/dt = (omega1, omega2, alpha1, alpha2)
```

`alpha1`, `alpha2`는 두 질량과 막대 제약이 결합된 비선형 가속도 식이다. 분모에는 `cos(2(theta1-theta2))`, 분자에는 중력항과 두 각속도의 제곱에 의한 원심·결합항이 들어간다. 작은 초기각 차이도 시간이 지나면 큰 궤적 차이로 성장할 수 있지만, 이것이 무작위 운동이라는 뜻은 아니다. 같은 초기 상태와 같은 적분 순서에서는 결정적으로 같은 결과가 나온다.

## 수치 적분

한 substep에서 고전적 RK4를 사용한다.

```text
k1 = f(y)
k2 = f(y + dt*k1/2)
k3 = f(y + dt*k2/2)
k4 = f(y + dt*k3)
y_next = y + dt*(k1 + 2k2 + 2k3 + k4)/6
```

`QueryPerformanceCounter`로 실제 경과 시간을 측정해 accumulator에 넣고 `dt=0.006s`만큼씩 적분한다. 평상시에는 현실 시간과 1:1로 진행한다. 창 이동이나 디버깅으로 오래 멈춘 경우 한 번에 최대 0.25초만 따라잡으며 초과 시간은 버린다. 일시정지·초기화 시 잔여 시간도 초기화하고 Space의 키 자동 반복은 무시한다. Win32 timer는 hard real-time clock이 아니므로 표시 간격 자체가 항상 일정한 것은 아니다.

RK4는 이 짧은 시각화에 충분히 정확하지만 symplectic integrator는 아니다. 감쇠가 없는 보존계에서 장시간 실행하면 수치 에너지 drift가 누적될 수 있다.

## 렌더링과 Win32 구조

1. `WinMain`이 창 클래스를 등록하고 900×700 창을 만든다.
2. `WM_CREATE`에서 16ms timer를 시작한다.
3. `WM_TIMER`가 물리를 적분하고 두 번째 추 위치를 ring buffer에 넣는다.
4. `InvalidateRect`가 `WM_PAINT`를 요청한다.
5. `WM_PAINT`는 memory DC와 compatible bitmap에 장면 전체를 그린다.
6. 마지막에 `BitBlt` 한 번으로 window DC에 복사해 깜빡임을 줄인다.

막대는 GDI pen, 추는 solid brush와 `Ellipse`, 궤적은 오래된 점에서 최근 점으로 갈수록 초록·파랑 성분을 밝게 한 선분으로 표시한다. GDI 기본 선에는 alpha blending을 사용하지 않으므로 코드의 `alpha` 지역 변수는 실제 투명도가 아니라 색상 밝기다.

## 주요 코드

| 위치 | 책임 |
|---|---|
| `State` | 두 각도와 두 각속도 |
| `derivatives` | 결합 운동방정식의 각가속도 계산 |
| `integrate` | RK4 한 단계 |
| `pendulum_points` | 물리 각도를 화면 좌표로 변환 |
| `push_trail` | 900개 원형 궤적 버퍼 |
| `draw_scene` | off-screen GDI 렌더링과 `BitBlt` |
| `window_proc` | timer, 키 입력, paint와 종료 메시지 |

## 현재 한계

- 점질량·질량 없는 강체 막대 모델이며 막대 자체 관성은 없다.
- 공기 저항, 축 마찰, 외력과 충돌이 없다.
- 장시간 에너지 보존을 별도로 측정하지 않는다.
- 0.25초보다 긴 지연은 물리 시간을 버려 UI 장기 정지를 방지하므로 정지 구간까지 현실 시간과 일치하지는 않는다.
- 한 프레임의 궤적 선분마다 GDI pen을 생성·삭제해 불필요한 객체 비용이 있다.
- 계산과 렌더링이 UI thread에서 실행되며 기록·재생·파라미터 UI가 없다.
- 창 크기가 작아지면 고정 185픽셀 막대가 잘릴 수 있다.

## 개선 로드맵

1. 물리 step 사이의 렌더 보간과 사용자 배속 조절
2. 총 에너지·각운동량 그래프와 수치 오차 표시
3. RK4, symplectic Euler, Verlet 계열 적분기 비교
4. 질량·길이·중력·감쇠·초기 조건 조절 UI
5. 두 초기 조건을 나란히 실행해 카오스 민감도 시각화
6. GDI 자원 캐시 또는 현대 GPU 렌더러로 궤적 비용 절감
7. CSV 상태 기록과 결정적 재생

## 참고 자료

저장소 루트의 `make check`는 10,000 RK4 step의 에너지 오차, 정지 평형, 실제 시간별 step 수와 일시정지를 검사한다. 테스트 기준과 한계는 [공통 테스트](../tests/README.md)에 있다.

- [myPhysicsLab: Double Pendulum equations and derivation](https://www.myphysicslab.com/pendulum/double-pendulum-en.html)
- [myPhysicsLab derivation PDF](https://www.myphysicslab.com/develop/docs/Double_Pendulum.pdf)
- [Microsoft: Windows messages and message queues](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
- [Microsoft: CreateCompatibleDC](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createcompatibledc)
