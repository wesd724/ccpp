# rotating-cube-c

그래픽 API 없이 C와 콘솔 문자만으로 회전하는 3D 큐브를 표시하는 소형 소프트웨어 렌더링 프로젝트다. 3차원 회전, 원근 투영, 프레임버퍼와 깊이 버퍼의 핵심 원리를 200줄 미만의 코드로 확인한다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 콘솔 그래픽스, 3D 수학, 소프트웨어 렌더링 |
| 언어 | C23 |
| 플랫폼 | Cygwin 또는 ANSI/ECMA-48 제어 시퀀스와 POSIX `nanosleep`을 지원하는 터미널 |
| 외부 라이브러리 | 없음, 시스템 수학 라이브러리 `libm`만 링크 |
| 출력 | 80×40 문자 프레임 |
| 목표 프레임 간격 | 30ms, 이론상 약 33 FPS |
| 구현 난이도 | 2/5 |
| 주요 파일 | [main.c](main.c), [Makefile](Makefile) |

## 빌드와 실행

```sh
make run
```

빌드만 하려면 `make`, 종료하려면 `Ctrl+C`, 생성물을 지우려면 `make clean`을 사용한다.

## 렌더링 과정

한 프레임은 다음 순서로 만들어진다.

1. 80×40 문자 배열을 공백으로, 같은 크기의 역깊이 배열을 0으로 초기화한다.
2. 큐브의 여섯 면을 `(u, v)` 격자로 샘플링한다.
3. 각 표면점을 X축, Y축, Z축 순서로 회전한다.
4. 카메라 앞쪽으로 평행 이동한 뒤 원근 투영한다.
5. 화면 셀에 기존 값보다 가까운 점만 기록한다.
6. ANSI 커서 이동 명령으로 화면 시작점으로 돌아가 전체 문자 배열을 출력한다.

면마다 `@`, `%`, `#`, `*`, `+`, `.` 문자를 다르게 사용하므로 조명 계산 없이도 방향을 구분할 수 있다.

## 수학과 알고리즘

### 3축 회전

점 `p = (x, y, z)`에 X, Y, Z 회전을 차례로 적용한다. 예를 들어 X축 회전은 다음과 같다.

```text
y' = y cos(ax) - z sin(ax)
z' = y sin(ax) + z cos(ax)
```

Y축과 Z축에도 같은 형태의 회전행렬을 적용한다. 회전은 교환법칙이 성립하지 않으므로 순서를 바꾸면 애니메이션 궤적도 달라진다.

### 원근 투영

회전한 점의 카메라 깊이를 `d = z + camera_distance`로 두고 다음과 같이 화면 좌표를 얻는다.

```text
screen_x = center_x + x / d * horizontal_scale
screen_y = center_y - y / d * vertical_scale
```

가까운 물체가 크게 보이는 원근 나눗셈이며, `d <= 0.1`인 카메라 뒤쪽 점은 버린다. 터미널 문자는 세로로 더 길게 보이는 경우가 많아 X축 배율을 두 배로 보정한다.

### 역깊이 Z-buffer

각 문자 셀에는 `1 / d`를 저장한다. 새 값이 기존 값보다 클 때만 문자를 덮어쓰므로 가까운 면이 먼 면을 가린다. 삼각형 rasterization 대신 표면을 촘촘히 점 샘플링한 근사 방식이다.

## C에서 확인할 수 있는 개념

- 고정 크기 전역 배열을 프레임버퍼로 사용
- 출력 매개변수 포인터를 이용한 3차원 변환 결과 반환
- `memset`, `fwrite`, `fflush` 기반 전체 프레임 출력
- `signal`과 `atexit`을 이용한 종료 처리
- `_POSIX_C_SOURCE`와 `nanosleep`을 이용한 프레임 지연
- 삼각함수와 부동소수점 좌표를 정수 화면 좌표로 변환

## 터미널 상태 관리

시작할 때 화면을 지우고 커서를 숨긴다. 정상 종료, `SIGINT`, `SIGTERM`에서는 커서를 다시 표시하고 색상 상태를 초기화한다. 프로그램을 강제 종료하면 터미널 커서 상태가 남을 수 있으며, 이 경우 터미널을 다시 열거나 `printf '\033[?25h'`에 해당하는 제어 시퀀스를 보내 복구할 수 있다.

신호 핸들러는 `volatile sig_atomic_t` 종료 플래그만 변경한다. 출력과 복구는 메인 루프 종료 후 `atexit`에서 한 번만 수행하며 신호 핸들러 안에서 `printf`나 `exit`을 호출하지 않는다. `make clean`은 Cygwin의 `.exe` 생성물도 제거한다.

## 현재 한계

- 큐브 표면을 삼각형이 아니라 점으로 샘플링하므로 샘플 간격에 따라 구멍과 aliasing이 생긴다.
- 조명, 법선, texture, clipping plane과 삼각형 rasterization이 없다.
- 화면 크기 80×40과 문자 종횡비를 고정 가정한다.
- 터미널 resize와 키보드 카메라 조작을 처리하지 않는다.
- 프레임 지연이 렌더링 시간을 보정하지 않으므로 실제 주기는 `렌더 시간 + 30ms`다.
- ANSI/ECMA-48 제어 시퀀스를 해석하지 않는 콘솔에서는 화면 갱신이 정상적으로 보이지 않는다.

## 개선 로드맵

1. 터미널 크기 감지와 동적 프레임버퍼
2. edge 또는 삼각형 기반 rasterizer와 back-face culling
3. 법선·광원 기반 문자 명암 팔레트
4. near-plane clipping과 더 일반적인 카메라 행렬
5. 프레임 시간 측정에 기반한 일정한 delta time
6. C 버전과 [C++ 버전](../rotating-cube-cpp/README.md)의 성능·구조 비교 테스트

## 참고 자료

- [ECMA-48: Control Functions for Coded Character Sets](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [Khronos OpenGL 4.6 reference](https://registry.khronos.org/OpenGL-Refpages/gl4/html/start.html): 현재 코드는 OpenGL을 쓰지 않지만, 투영·깊이·rasterization을 확장할 때 비교 기준으로 사용한다.
