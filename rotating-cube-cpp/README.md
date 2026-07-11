# rotating-cube-cpp

C++20과 콘솔 문자로 회전하는 3D 큐브를 그린다. [C 구현](../rotating-cube-c/README.md)과 같은 수학·출력 방식을 유지하면서, 값 타입, 클래스, RAII와 표준 라이브러리로 상태와 자원 수명을 표현하는 방법을 비교한다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 콘솔 그래픽스, 3D 수학, C/C++ 설계 비교 |
| 언어 | C++20 |
| 플랫폼 | Cygwin 또는 ANSI/ECMA-48 제어 시퀀스를 지원하는 터미널 |
| 외부 라이브러리 | 없음 |
| 주요 표준 라이브러리 | `array`, `atomic`, `chrono`, `thread`, `iostream` |
| 출력 | 80×40 문자 프레임, 목표 간격 30ms |
| 구현 난이도 | 2/5 |
| 주요 파일 | [main.cpp](main.cpp), [Makefile](Makefile) |

## 빌드와 실행

```sh
make run
```

종료는 `Ctrl+C`, 정리는 `make clean`이다.

## 알고리즘

렌더링 알고리즘은 C 버전과 같다.

1. 큐브의 여섯 면을 점 격자로 샘플링한다.
2. X·Y·Z 회전을 순서대로 적용한다.
3. `x / depth`, `y / depth` 원근 투영으로 화면 좌표를 얻는다.
4. 각 문자 셀의 `1 / depth`와 비교해 가장 가까운 면 문자만 남긴다.
5. ANSI 커서 이동으로 프레임을 같은 위치에 덮어쓴다.

표면 간격은 `0.16`으로 C 버전의 `0.18`보다 조금 촘촘하다. 따라서 한 면에 더 많은 점을 평가하지만 알고리즘의 성격은 같다.

## C++ 설계 요소

### 값 타입

`Vec3`와 `Angles`는 기본값을 가진 단순 값 타입이다. 회전 함수는 여러 출력 포인터 대신 변환된 `Vec3`를 값으로 반환한다.

### RAII 터미널 관리

`Terminal` 생성자는 화면을 지우고 커서를 숨기며, 소멸자는 커서를 복구한다. 복사를 금지해 동일한 터미널 상태 관리자 인스턴스가 중복 생성되는 실수를 줄인다. `main`이 정상 범위를 벗어나면 소멸자가 자동으로 실행된다.

### 렌더러 상태 캡슐화

`CubeRenderer`가 `std::array` 기반 문자 버퍼와 깊이 버퍼를 소유한다. 렌더링 함수 밖에서 이 배열을 직접 수정할 수 없으므로 C 버전의 전역 상태보다 소유권 경계가 명확하다.

### 사용한 C++ 기능

- `std::array::fill`을 이용한 고정 배열 초기화
- `std::chrono_literals`의 `30ms`
- `std::this_thread::sleep_for`
- `std::atomic_bool` 실행 상태
- `const auto [point, shade]` 구조적 바인딩
- `[[nodiscard]]`로 변환 결과 무시 방지
- 익명 namespace로 번역 단위 내부 심볼 제한

## 수학적 핵심

회전은 세 개의 3×3 회전행렬을 순차 적용한 것과 같다. 투영은 카메라 깊이 `d`에 대해 `x/d`, `y/d`를 사용하고, visibility는 역깊이 `1/d`의 최대값으로 결정한다. 회전 순서와 문자 종횡비 보정의 의미는 [C 버전 문서](../rotating-cube-c/README.md#수학과-알고리즘)에 자세히 설명되어 있다.

## C 버전과의 차이

| 관점 | C 버전 | C++ 버전 |
|---|---|---|
| 화면 상태 | 전역 배열 | `CubeRenderer` 멤버 |
| 점 반환 | 출력 포인터 3개 | `Vec3` 값 반환 |
| 터미널 복구 | `atexit` 함수 | RAII 소멸자 |
| 시간 | POSIX `nanosleep` | `std::chrono`, `std::thread` |
| 실행 상태 | 무한 루프와 signal 종료 | atomic 실행 플래그 |
| 고정 배열 | C 배열 | `std::array` |

두 구현의 화질 차이는 언어 때문이 아니라 `SURFACE_STEP` 값 차이 때문이다. C++이 자동으로 더 빠르거나 더 선명한 것은 아니다.

## 현재 한계

- 점 샘플링이라 삼각형의 연속 면을 정확히 rasterize하지 않는다.
- terminal resize, 입력 카메라, 조명과 clipping이 없다.
- `std::cout` 전체 프레임 출력은 고성능 terminal renderer가 아니다.
- 목표 30ms 지연에 렌더 시간이 추가되어 정확한 고정 FPS가 아니다.
- 비정상 강제 종료에서는 RAII 소멸자가 실행되지 않아 커서가 숨은 채 남을 수 있다.
- 현재 C++20으로 빌드하며 C++23 전환과 그에 맞는 기능 사용은 아직 하지 않았다.

## 개선 로드맵

1. C++23 빌드 검증과 표준 기능 적용 여부 결정
2. 동적 terminal 크기와 `std::vector` 프레임버퍼
3. 행렬·카메라·삼각형 모듈 분리
4. back-face culling, clipping, 문자 명암과 anti-aliasing
5. C 버전과 동일 설정의 benchmark와 출력 회귀 테스트
6. signal 처리와 terminal 복구를 플랫폼 계층으로 분리

## 참고 자료

- [ECMA-48: Control Functions for Coded Character Sets](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [C++ reference: `std::array`](https://en.cppreference.com/w/cpp/container/array)
- [C++ reference: RAII](https://en.cppreference.com/w/cpp/language/raii)
