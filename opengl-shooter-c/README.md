# opengl-shooter-c

C, Win32/WGL과 OpenGL로 만든 2D top-down 아케이드 슈팅 게임이다. 마우스 방향 조준, 연속 발사, 추적형 적, 점수·체력·게임 오버와 짧은 입자 효과를 하나의 실시간 루프에서 관리한다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 2D 게임, 실시간 시뮬레이션, OpenGL |
| 언어 | C23 |
| 플랫폼 | Windows desktop, Win32, WGL |
| 논리 해상도 | 1000×720 고정 |
| 그래픽 경로 | `modern_gl_compat.h`를 통한 GLSL 3.30 + VAO/VBO 변환 |
| 상태 관리 | 고정 크기 객체 풀 |
| 충돌 | 원-원 거리 검사 |
| 외부 라이브러리 | 없음, `opengl32`, `gdi32`, `user32`, `libm` |
| 구현 난이도 | 3/5 |
| 주요 파일 | [main.c](main.c), [Makefile](Makefile), [modern_gl_compat.h](../modern_gl_compat.h) |

## 빌드와 실행

```sh
make run
```

| 입력 | 동작 |
|---|---|
| `WASD` 또는 방향키 | 플레이어 이동 |
| 마우스 이동 | 조준 방향 변경 |
| 왼쪽 버튼 또는 `Space` 유지 | 0.10초 간격 연속 발사 |
| `R` | 점수·체력·객체 상태 초기화 |
| `Esc` | 종료 |

## 게임 규칙

- 플레이어는 화면 안에서 초당 360 logical pixel 속도로 이동한다.
- 탄환은 마우스 방향의 단위벡터를 따라 초당 780 pixel로 진행하고 1.35초 뒤 사라진다.
- 적은 화면 위쪽에서 생성되어 플레이어 방향으로 속도를 조금씩 보정한다.
- 탄환과 적이 겹치면 적이 제거되고 10점을 얻는다.
- 적과 플레이어가 겹치면 체력이 1 감소하며, 초기 체력 5가 모두 사라지면 game over다.
- 점수가 높아질수록 적 속도가 조금 증가하고 생성 간격이 짧아진다.

창 제목이 점수, 체력과 game over 상태를 표시한다. 별도의 text renderer나 메뉴 UI는 없다.

## 상태와 객체 풀

동적 allocation 대신 최대 개수를 가진 정적 배열을 사용한다.

| 종류 | 최대 개수 | 핵심 상태 |
|---|---:|---|
| 탄환 | 96 | 활성 여부, 위치·속도, 반지름, 남은 수명 |
| 적 | 48 | 위치·속도, 반지름, 기본 속도, 회전 위상 |
| 파티클 | 220 | 위치·속도, 수명, 크기, 색 |

생성할 때 첫 번째 비활성 slot을 선형 탐색한다. 메모리 사용이 예측 가능하고 해제가 필요 없지만, pool이 가득 차면 새 객체는 생성되지 않는다.

## 벡터와 이동

조준 방향은 다음과 같다.

```text
direction = normalize(mouse_position - player_position)
```

대각선 이동도 먼저 정규화하므로 X와 Y를 동시에 눌러도 축 하나만 누를 때보다 빨라지지 않는다. 위치 갱신은 기본적인 explicit Euler 형태다.

```text
position_next = position + velocity * dt
```

`QueryPerformanceCounter`로 프레임 간 시간을 측정하고, 창 이동이나 breakpoint 뒤 큰 이동이 생기지 않도록 `dt`를 최대 0.05초로 제한한다.

## 충돌 알고리즘

모든 플레이 요소를 원으로 근사한다.

```text
collision if distance(a, b) < radius_a + radius_b
```

탄환-적 검사는 활성 적마다 활성 탄환을 모두 순회한다. 최악의 경우 `48×96 = 4608`번 거리 검사를 하므로 현재 규모에서는 충분하다. 더 큰 게임에서는 uniform grid, quadtree 또는 broad phase가 필요하다.

## 렌더링

- 플레이어: 조준 방향과 수직 right vector로 만든 삼각형
- 적·탄환·파티클: triangle fan 원
- 배경: 50 pixel 간격 line grid
- blending: `SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA`
- double buffering: WGL back buffer와 `SwapBuffers`

### OpenGL 호환 레이어의 정확한 상태

소스에는 `glBegin`, `glVertex2f`, `glColor4f`, `glOrtho`가 보이지만 [modern_gl_compat.h](../modern_gl_compat.h)가 이 이름을 매크로로 바꾼다. 정점은 CPU 임시 배열에 모이고 clip-space로 변환된 뒤 GLSL 3.30 shader, VAO와 동적 VBO를 통해 `glDrawArrays`로 제출된다. Windows 기본 `opengl32`가 직접 노출하지 않는 함수는 `wglGetProcAddress`로 읽는다.

따라서 최종 draw는 shader와 VBO를 사용하지만 다음 이유로 완전한 현대 OpenGL 설계라고 보기는 어렵다.

- immediate-style 호출 하나마다 CPU 배열을 다시 구성한다.
- draw마다 `glBufferData`로 VBO 내용을 재업로드한다.
- 행렬을 GPU uniform이 아니라 CPU에서 정점마다 적용한다.
- shader source가 공용 header 문자열 안에 들어 있다.
- 명시적인 OpenGL core context를 생성하지 않는다.

이 레이어는 기존 코드를 실행 가능한 중간 상태로 유지하기 위한 것이며 새 프로젝트의 권장 API가 아니다.

## Win32 실시간 루프

GUI timer 대신 `PeekMessage`로 pending message를 모두 처리한 뒤 매 반복마다 update와 render를 실행한다.

```text
poll messages -> measure dt -> update game -> render -> Sleep(1)
```

키 상태 배열과 `GetAsyncKeyState`를 함께 사용해 key message가 누락되거나 focus 전환 직후에도 입력을 보완한다.

입력 polling과 게임 갱신은 이 창이 foreground일 때만 수행한다. 포커스를 잃으면 눌린 키·발사 상태를 지우고, capture 취소 시에도 발사를 멈춘다. 창이 파괴된 뒤에는 추가 update/render 없이 루프를 종료하며 GL 자원은 context를 해제하기 전에 정리한다.

논리 좌표는 1000×720을 유지하되 viewport는 실제 client 크기에 맞추고 마우스 좌표는 역변환한다. 적 추적의 지수 평활과 파티클 감쇠는 `dt`에 맞춰 60Hz 기준 계수를 보정한다. 이 보정이 variable-step 이동과 충돌까지 완전히 frame-independent하게 만드는 것은 아니다.

## 현재 한계

- 창 크기에 맞춰 논리 화면 전체를 늘리므로 원래 종횡비와 다르게 resize하면 도형이 늘어날 수 있다.
- texture, sprite atlas, text HUD, audio와 animation asset이 없다.
- 모든 게임·플랫폼·렌더링 코드가 `main.c` 한 파일에 결합되어 있다.
- 충돌은 frame 간 swept test가 아닌 현재 위치 원 검사라 매우 큰 `dt`에서는 tunneling 가능성이 있다.
- 적 행동은 단순 추적뿐이며 pathfinding·상태 머신이 없다.
- random seed가 현재 시간이라 실행 replay가 결정적이지 않다.
- VSync 설정과 명시적인 frame cap이 없고 `Sleep(1)`만 사용한다.
- 공용 호환 레이어는 성능·기능 면에서 임시 구조다.

## 개선 로드맵

1. native GLSL 파일, shader program, persistent VAO/VBO 기반 batch renderer
2. Win32 platform, renderer, game state, collision과 effect 모듈 분리
3. 종횡비를 유지하는 letterbox viewport와 대응 입력 변환
4. texture atlas, text HUD, audio와 game menu
5. swept collision 또는 고정 물리 step
6. 적 유형·weapon·wave·pickup과 deterministic seed/replay
7. unit test 가능한 순수 게임 로직과 render regression test

## 참고 자료

루트 `make check`는 포커스·capture 취소와 resize 입력을, `make check-graphics`는 공용 GL 레이어의 실제 삼각형 픽셀과 재초기화를 검사한다. GL 함수 전체의 로딩 성공 여부를 확인하고 shader compile/link 실패 시 중간 자원을 정리한다. [공통 테스트](../tests/README.md)를 참고한다.

- [Khronos OpenGL Registry](https://registry.khronos.org/OpenGL/index_gl.php)
- [OpenGL 4.6 core reference pages](https://registry.khronos.org/OpenGL-Refpages/gl4/html/start.html)
- [Microsoft: Windows messages and message queues](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
