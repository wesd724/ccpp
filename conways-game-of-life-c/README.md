# conways-game-of-life-c

콘웨이의 생명 게임(Conway's Game of Life)을 ANSI 콘솔에서 실시간으로 실행하고 직접 편집하는 C23 프로젝트다. 프로그램을 시작하면 주기 3의 `Pulsar`가 재생되며, 실행 중 셀을 켜고 끄거나 대표 패턴을 배치하고 세대 진행 속도와 경계 조건을 바꿀 수 있다.

생명 게임은 플레이어가 승패를 겨루는 일반적인 게임이 아니라, 같은 국소 규칙을 모든 셀에 반복 적용했을 때 정지물, 진동자, 이동 물체와 복잡한 구조가 나타나는 이산 동역학계다. 그래픽 API나 외부 라이브러리는 사용하지 않는다. Cygwin의 POSIX 터미널 API로 입력을 받고 ECMA-48/ANSI 제어 시퀀스로 화면을 갱신한다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 셀룰러 오토마타, 이산수학, 콘솔 시각화 |
| 언어·표준 | C23 |
| 규칙 | Conway Life `B3/S23`, Moore 이웃 8개 |
| 경계 조건 | 바깥을 죽은 셀로 보는 유한 평면, 양쪽 끝이 이어지는 토러스 |
| 초기 패턴 | Pulsar |
| 내장 패턴 | Block, Blinker, Glider, Pulsar, Gosper glider gun |
| 표시 속도 | 최대 30 FPS |
| 세대 속도 | 1~60 generation/s, 기본 8 generation/s |
| 외부 라이브러리 | 없음 |
| 현재 플랫폼 | Cygwin의 ANSI 호환 대화형 터미널 |
| 구현 난이도 | 2/5 |

## 빌드와 실행

```sh
make run
```

핵심 규칙, 패턴, 경계 조건과 격자 크기 변경을 자동 검증하려면 다음 명령을 실행한다.

```sh
make check
```

검증이 성공하면 다음 결과가 나온다.

```text
All 11 Conway Life tests passed.
```

프로그램은 키 입력을 즉시 읽을 수 있는 대화형 ANSI 터미널이 필요하다. 일반적인 Cygwin 터미널이나 VS Code 통합 터미널에서 실행할 수 있으며, 전체 HUD를 한 화면에서 보려면 80×24 이상의 터미널을 권장한다. 창 크기가 바뀌면 격자를 중앙 기준으로 확대하거나 축소하면서 남아 있는 셀을 보존한다.

회귀 검증에는 중심 셀과 이웃을 포함한 512가지 상태, population 변화, 셀 나이 포화, Glider의 4세대 이동, Pulsar의 주기 3과 잘못된 크기 요청도 포함한다. 루트 `make check`는 수정키 방향키·application cursor·기능키 escape sequence 처리도 검사한다. 기능키의 숫자 매개변수는 패턴 배치 명령으로 해석하지 않는다. [공통 테스트](../tests/README.md)를 참고한다.

## 조작법

| 입력 | 동작 |
|---|---|
| `Space`, `P` | 실행과 일시정지 전환 |
| `N`, `.` | 일시정지하고 정확히 한 세대 진행 |
| 방향키, `H J K L` | 일시정지하고 편집 커서 이동 |
| `Enter` | 커서 위치 셀의 생존 상태 전환 |
| `+`, `-` | 세대 진행 속도를 1 generation/s씩 조절 |
| `R` | 밀도 24%의 의사난수 상태 생성 |
| `C` | 모든 셀과 세대 통계 초기화 |
| `B` | `DEAD`와 `WRAP` 경계 조건 전환 |
| `1` | 2×2 정지물 Block 배치 |
| `2` | 주기 2 진동자 Blinker 배치 |
| `3` | 대각선으로 이동하는 Glider 배치 |
| `4` | 주기 3 진동자 Pulsar 배치 |
| `5` | Glider를 계속 만드는 Gosper glider gun 배치 |
| `Q`, `Esc` | 종료 |

방향키를 누르면 자동으로 일시정지하고 커서가 나타난다. 패턴 키는 기존 셀을 지우지 않고 현재 커서를 중심으로 패턴을 겹쳐 놓으며, 격자 밖으로 나간 부분은 잘린다. 빈 격자에서 패턴 하나만 관찰하려면 `C`를 누른 뒤 원하는 숫자 키를 누른다. 편집 커서는 가장자리에서 반대편으로 이동하지만, 셀의 물리 경계 조건은 `B`로 별도 선택한다.

화면의 `Born`과 `Died`는 가장 최근 세대에서 태어나거나 죽은 셀 수다. 새로 태어난 셀은 밝은 청록색, 생존한 지 얼마 안 된 셀은 밝은 녹색, 오래 생존한 셀은 녹색으로 표시한다. 나이는 시각화에만 사용하며 생명 규칙에는 영향을 주지 않는다.

## 생명 규칙

각 셀은 가로, 세로와 대각선으로 맞닿은 여덟 개의 Moore 이웃을 센다. 현재 셀의 상태를 `a`로, 살아 있는 이웃 수를 `n`으로 두면 다음 세대 상태는 다음과 같다.

```text
alive_next = (a and (n = 2 or n = 3)) or ((not a) and n = 3)
```

- 살아 있는 셀은 이웃이 2개나 3개면 생존한다. 이웃이 더 적으면 과소밀도, 더 많으면 과밀도로 죽는다.
- 죽은 셀은 이웃이 정확히 3개면 태어난다.
- `B3/S23`은 `Birth 3 / Survival 2 or 3`을 줄인 rulestring이다.

한 셀을 계산하자마자 원래 격자를 바꾸면 뒤에 계산하는 셀이 이미 바뀐 상태를 읽게 된다. 이 구현은 현재 세대 `cells`와 다음 세대 `next_cells`를 분리하고 모든 셀 계산이 끝난 뒤 포인터를 교환한다. 따라서 모든 셀이 논리적으로 동시에 갱신된다.

## 경계 조건

### DEAD

격자 바깥을 항상 죽은 셀로 본다. 무한 평면의 작은 영역만 잘라 관찰하는 것과 비슷하지만, 가장자리에 도달한 패턴은 일부 이웃을 잃고 소멸하거나 변형될 수 있다. 프로그램의 기본값이다.

### WRAP

왼쪽과 오른쪽, 위와 아래를 서로 이어 붙인다. 격자의 위상은 토러스가 되며, Glider처럼 이동하는 패턴이 한쪽 경계를 나가 반대쪽에서 다시 나타난다.

## 알고리즘과 데이터 흐름

```text
비차단 키 입력
  -> 편집·패턴·속도·경계 상태 갱신
  -> 고정된 generation 간격만큼 B3/S23 계산
  -> cells / next_cells 포인터 교환
  -> 셀 나이에 따른 ANSI 색상 프레임 생성
  -> 최대 30 FPS로 터미널에 한 번에 출력
```

격자가 `W×H`일 때 한 세대는 각 셀의 이웃 8개를 고정 횟수로 확인하므로 시간 복잡도는 `O(W×H)`다. 셀 나이를 담는 1바이트 배열 두 개를 사용하므로 핵심 격자 메모리는 약 `2×W×H`바이트다. 렌더링 속도와 세대 진행 속도를 분리했기 때문에 낮은 세대 속도에서도 입력과 화면 갱신은 부드럽게 유지된다. 프레임이 크게 지연됐을 때 한 번에 최대 8세대만 따라잡아 입력이 장시간 멈추는 것도 방지한다.

난수 초기화에는 전역 `rand()` 대신 명시적인 64비트 xorshift 상태를 사용한다. 같은 seed와 밀도를 넘기면 같은 격자가 생성되므로 테스트와 실험을 재현할 수 있다. 사용자용 `R` 입력은 매번 seed를 바꿔 다른 상태를 만든다.

## 주요 파일

| 파일 | 책임 |
|---|---|
| [src/main.c](src/main.c) | 실행 루프, 입력 상태기계, 세대 타이밍, HUD와 ANSI 프레임 생성 |
| [src/life.c](src/life.c) | 격자 수명, B3/S23 계산, 경계 표본화, 난수화, 중앙 기준 크기 변경 |
| [src/patterns.c](src/patterns.c) | 다섯 내장 패턴 데이터와 커서 중심 배치 |
| [src/terminal.c](src/terminal.c) | raw terminal 진입·복구, 비차단 입력, 창 크기, 단조 시간과 출력 |
| [include/life.h](include/life.h) | 생명 격자와 통계 API |
| [include/patterns.h](include/patterns.h) | 패턴 조회·배치 API |
| [include/terminal.h](include/terminal.h) | 터미널 플랫폼 계층 API |
| [tests/test_life.c](tests/test_life.c) | 정지물, 진동자, 생존·사망, 토러스, 패턴 수, 난수와 resize 회귀 테스트 |
| [Makefile](Makefile) | C23 빌드, 실행, 테스트와 생성물 정리 |

시뮬레이션 핵심은 터미널 모듈을 참조하지 않는다. 따라서 이후 GUI나 OpenGL 프런트엔드를 추가하더라도 `life.c`와 테스트를 그대로 재사용할 수 있다. 현재 규모에는 별도 프로젝트 `AGENTS.md`가 필요하지 않아 루트 규칙만 따른다.

## 현재 한계

- 유한 크기 격자를 직접 순회하므로 매우 큰 희소 패턴에는 비효율적이다.
- RLE과 Life 1.05/1.06 패턴 파일을 불러오거나 저장하지 못한다.
- 실행 취소, 패턴 회전·반전, 선택 영역 복사와 붙여넣기는 없다.
- 셀 하나가 콘솔 문자 두 칸을 사용하며 자유로운 확대·축소나 카메라 이동은 없다.
- 규칙은 `B3/S23`으로 고정되어 있어 HighLife 같은 다른 life-like cellular automaton을 입력할 수 없다.
- ANSI raw terminal을 사용하는 현재 프런트엔드는 네이티브 Windows 콘솔만 사용하는 도구 체인이나 출력 리디렉션 환경에 적합하지 않다.

## 개선 방향

1. 표준 RLE parser와 pattern library, 파일 저장·불러오기
2. rulestring parser를 통한 `B/S` 계열 셀룰러 오토마타 실험
3. 선택 영역, 회전·반전, undo/redo와 패턴 검색 UI
4. viewport와 zoom을 갖춘 현대 OpenGL 또는 네이티브 GUI 프런트엔드
5. 큰 밀집 격자를 위한 bit-packed 행, SIMD와 다중 스레드 갱신
6. 거대한 희소 우주와 장기 세대 건너뛰기를 위한 quadtree/HashLife 백엔드
7. 반복 상태 탐지, 주기 측정, population 그래프와 CSV 기록

## 참고 자료

- [LifeWiki: Conway's Game of Life](https://conwaylife.com/wiki/Conway%27s_Game_of_Life)
- [LifeWiki: Rulestring](https://conwaylife.com/wiki/Rulestring)
- [LifeWiki: Gosper glider gun](https://conwaylife.com/wiki/Gosper_glider_gun)
- [ECMA-48 terminal control functions](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
