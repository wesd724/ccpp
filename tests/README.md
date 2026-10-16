# 저장소 회귀 테스트

루트 `make`는 열 개 실행 프로젝트를 빌드한다. `make check`는 다음 여덟 테스트 실행 파일과 Conway의 열한 개 규칙 테스트를 실행하며 실패하면 0이 아닌 종료 코드로 중단한다. Windows/Cygwin과 GCC/G++가 필요하다. GPU와 오디오 endpoint는 기본 검사에 필요하지 않지만 Win32 라이브러리와 hidden window 생성은 사용한다.

| 테스트 | 검증 대상 |
|---|---|
| `test_fluid_input.c` | 2D 브러시의 배열 guard·ghost cell 보존, 창 바깥 좌표, 이동량 단일 소비, resize·최소화·capture 취소 |
| `test_ray_geometry.c` | 카메라 좌우·상하, 단위 광선, 구·평면 교차, 바닥 재질 선택, hidden window에서 첫 8행 batch와 Esc 종료 |
| `test_pendulum.c` | RK4 10,000 step의 유한 에너지와 초기값 대비 절대 오차 0.01 미만, 정지 평형, 1초의 고정 step 수, 일시정지 |
| `test_shooter_input.c` | 2D 슈팅의 포커스·capture 손실, 키 인덱스 범위와 resize 조준 좌표 |
| `test_shooter3d_input.c` | 3D 슈팅의 포커스 손실 시 키·발사 해제와 키 범위 |
| `test_platform_input.c` | 3D 유체 플랫폼의 포커스·capture 취소, 카메라 delta와 drag release 이벤트 |
| `test_audio.cpp` | 잘못된 PCM 형식과 GUID 거부, 24-in-32 PCM 하위 8비트, 네 음색의 유한·비영 파형과 release, 여덟 드럼의 비영 에너지·tail 종료 |
| `test_conway_input.c` | CSI 숫자 매개변수가 명령으로 새지 않음, 수정키 방향키와 application cursor |
| [Conway 규칙 테스트](../conways-game-of-life-c/tests/test_life.c) | 512가지 국소 상태, population 불변식, 경계·패턴·주기·나이 포화·난수·resize·잘못된 크기 |

일부 기존 프로그램이 단일 소스 또는 비공개 구현을 사용하므로 white-box 테스트는 해당 `.c`/`.cpp`를 테스트 번역 단위에 한 번만 포함한다. 이 파일을 별도로 중복 링크하지 않으며 실제 GUI 진입점은 호출하지 않는다. production 코드에서 구현 파일을 include하도록 권장하는 설계가 아니다. 검증만을 위해 모든 소형 프로그램의 공개 API를 늘리지 않았으며, 향후 모듈화 시 순수 코어 헤더를 이용하는 테스트로 옮길 수 있다. GCC의 include 상황에서 발생하는 `-Wsubobject-linkage` 억제는 오디오 테스트에만 한정한다.

## 장치 검사

```sh
make check-graphics
make check-audio
```

`check-graphics`는 실제 WGL context와 공용 GL renderer를 초기화해 삼각형을 그리고 픽셀을 읽는다. shader compile/link, OpenGL 오류, 자원 해제 후 재초기화를 검사한다. 지원 드라이버가 없으면 실패하며 조용히 검사를 건너뛰지 않는다. hidden 64×64 drawable 검사로, 전체 게임·3D 유체의 모든 모드를 화면 비교하는 테스트는 아니다.

`check-audio`는 실제 기본 WASAPI endpoint를 열어 sample rate와 최소 period를 조회한다. 연주 음을 재생하거나 전체 오디오 지연·청감 품질을 평가하는 검사는 아니다.

## 범위와 한계

- 회귀 테스트는 발견한 버그를 막는 데 집중한다. 전체 애플리케이션이 무결하다는 증명은 아니다.
- 배열 guard 검사는 주입 영역의 메모리 경계를 검사하며 전체 프로그램의 sanitizer 실행을 대체하지 않는다.
- RK4 에너지 허용 오차는 현재 초기 조건·60초 구간에 대한 회귀 기준이지 모든 상태의 정확도 보장이 아니다.
- GPU FLIP/MPM의 질량·발산·에너지, 긴 시간 안정성, 모든 해상도의 화면 품질과 frame pacing은 별도 검증 과제다.
- 작은 터미널·모든 실제 키보드 배열·장치 재연결·오디오 예외 경로 전체는 아직 자동화하지 않았다.

각 Makefile은 수정된 설정에 따른 재빌드를 지원하고, 두 shooter는 공용 GL 헤더를 빌드 의존성으로 추적한다. root `.gitignore`는 이후 생성되는 빌드 산출물을 제외하지만 이미 Git에서 추적하는 기존 바이너리를 자동으로 추적 해제하지는 않는다.
