# ray-tracer-c

C로 작성한 단일 스레드 CPU Whitted-style 레이트레이서다. OpenGL이나 GPU rasterization 없이 각 픽셀에서 카메라 광선을 만들고, 구·평면과의 교차, 직접광, hard shadow와 재귀 반사를 계산해 720×405 영상을 생성한다.

이 프로젝트는 광선을 사용하지만 Monte Carlo path tracer는 아니다. 간접 확산광, 확률적 재질 샘플링과 다중 bounce global illumination 대신 결정적인 직접광과 mirror reflection을 사용한다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 컴퓨터 그래픽스, 기하 광학, 오프라인 렌더링 |
| 언어 | C17 |
| 플랫폼·표시 | Win32 창 + GDI `StretchDIBits` |
| 렌더 해상도 | 720×405 |
| 표본 수 | 픽셀당 고정 2×2, 총 4 samples |
| 최대 재귀 깊이 | 4 |
| 장면 | 구 4개, 무한 평면 1개, 점광원 1개 |
| 출력 | 창 표시, binary PPM P6 저장 |
| 외부 라이브러리 | 없음, `gdi32`, `user32`, `libm` |
| 구현 난이도 | 이론 3/5, 구현 3/5 |
| 주요 파일 | [main.c](main.c), [Makefile](Makefile) |

## 빌드와 실행

```sh
make run
```

| 입력 | 동작 |
|---|---|
| `R` | 전체 영상을 다시 계산 |
| `S` | 현재 영상의 `ray_tracer_output.ppm` 저장 |
| `Esc` | 종료 |

## 렌더링 파이프라인

1. 픽셀 안의 네 고정 subpixel 위치에서 `camera_ray`를 만든다.
2. 각 광선과 장면의 모든 구·평면을 검사해 가장 작은 양의 `t`를 찾는다.
3. hit가 없으면 수직 방향에 따른 sky gradient를 반환한다.
4. hit 지점에서 ambient, Lambert diffuse와 Blinn-Phong specular를 계산한다.
5. hit에서 광원까지 shadow ray를 보내 다른 물체에 막히는지 검사한다.
6. 재질 반사율이 0보다 크면 반사 광선을 재귀 호출한다.
7. 네 표본을 평균하고 `1/2.2` power gamma를 적용해 8비트 BGRA로 저장한다.
8. CPU 픽셀 배열을 GDI로 표시하거나 PPM 파일로 기록한다.

## 광선과 교차

광선은 다음 매개변수식이다.

```text
P(t) = origin + t * direction,  t > 0
```

### 구 교차

구 중심 `C`, 반지름 `r`에 대해 `|P(t)-C|^2 = r^2`를 대입해 2차방정식을 얻는다. 코드는 `half_b` 형태로 판별식을 계산하고 가까운 근을 먼저 검사한다. 가까운 근이 허용 범위 밖이면 먼 근을 검사한다.

```text
normal = normalize(hit_point - sphere_center)
```

### 평면 교차

평면 위 점 `Q`와 단위 법선 `N`에 대해 다음을 사용한다.

```text
t = dot(Q - origin, N) / dot(N, direction)
```

분모가 0에 가까우면 광선과 평면이 평행하므로 교차하지 않는 것으로 처리한다.

### Self-intersection 방지

shadow와 reflection ray의 시작점을 표면 법선 또는 광선 방향으로 `EPSILON`보다 조금 이동한다. 부동소수점 오차 때문에 방금 맞은 표면을 다시 즉시 맞는 shadow acne를 줄이기 위한 bias다.

## 조명 모델

현재 직접광은 다음 세 항의 합이다.

```text
color = ambient + diffuse + specular
diffuse  = base_color * max(dot(N, L), 0) * material.diffuse
specular = light_color * pow(max(dot(N, H), 0), 80) * material.specular
```

`H`는 광원 방향과 view 방향의 half vector다. shadow ray가 가려지면 diffuse와 specular를 완전히 0으로 만들지 않고 `0.23`배로 줄인다. 따라서 물리적으로 완전히 어두운 umbra라기보다 가독성을 위한 경험적 fill light가 남는다.

재귀 반사는 입사 방향 `I`와 법선 `N`으로 다음 방향을 만든다.

```text
R = I - 2 dot(I, N) N
```

반사율로 local shading과 reflected color를 선형 혼합한다. Fresnel, rough reflection과 에너지 보존 BRDF는 없다.

## 카메라와 표본

- 위치: `(0, 1.35, 1.55)`
- 바라보는 기준점: `(0, 0.85, -4.2)`
- 수직 FOV: 55도
- aspect ratio: 720/405

forward, right, up 기저를 만든 뒤 viewport의 lower-left와 수평·수직 벡터로 각 표본 광선을 구성한다. 표본 위치는 픽셀 사분면 중심 `(0.25,0.25)`, `(0.75,0.25)`, `(0.25,0.75)`, `(0.75,0.75)`로 고정되어 있어 실행할 때마다 같은 결과가 나온다.

## 장면과 재질

`Material`은 base color, diffuse 계수, specular 계수와 reflective 비율을 가진다. 장면에는 서로 다른 크기와 색의 구 네 개, checker와 얇은 줄무늬를 절차적으로 계산하는 바닥, 수직 방향 sky gradient가 있다.

바닥 무늬는 texture image가 아니라 hit point의 `x`, `z` 좌표를 `floor`와 `fmod`에 넣어 계산한다.

## Win32와 출력 구조

- `WM_CREATE`가 사용자 메시지 `WM_APP+1`을 post한다.
- `WM_APP+1` handler가 UI thread에서 전체 영상을 렌더링한다.
- 24개 scanline마다 paint를 강제로 갱신해 진행 중인 일부 결과를 표시한다.
- 렌더가 끝나면 창 제목을 조작 안내로 변경한다.
- `S`는 Netpbm PPM의 `P6` header와 RGB bytes를 저장한다.

PPM은 단순하고 무손실이지만 일반 Windows 이미지 뷰어가 바로 열지 못할 수 있다.

## 계산 복잡도

공간 가속 구조가 없으므로 기본 비용은 대략 다음과 같다.

```text
pixels × samples × recursive rays × scene primitives
```

현재 primitive가 다섯 개뿐이라 단순 순회가 충분하다. 수백·수천 개 물체나 triangle mesh를 추가하면 BVH 같은 acceleration structure가 필요하다.

## 현재 한계

- 단일 CPU thread라 코어를 하나만 사용한다.
- 렌더링이 UI thread에서 실행되어 계산 중 입력 응답이 제한된다.
- 구와 평면만 지원하고 triangle, mesh, texture mapping과 BVH가 없다.
- hard shadow만 있으며 area light, soft shadow와 depth of field가 없다.
- refraction, dielectric, Fresnel, physically based BRDF가 없다.
- 간접광, caustics, participating media와 path tracing이 없다.
- 고정 4-sample 패턴이라 edge aliasing은 줄지만 temporal/random noise 분석은 할 수 없다.
- 출력은 8비트 PPM이며 HDR framebuffer와 tone mapping이 없다.
- 진행률, 렌더 시간과 thread 취소 기능이 없다.

## 개선 로드맵

1. scanline 또는 tile 단위 멀티스레딩과 진행률 표시
2. triangle mesh, OBJ loader와 BVH
3. area light와 stratified/random sampling
4. dielectric refraction, Schlick Fresnel과 rough metal
5. linear HDR accumulation과 tone mapping
6. path tracing, Russian roulette와 importance sampling 모드
7. deterministic image regression test와 성능 benchmark
8. 카메라·장면·재질·renderer를 별도 모듈로 분리

## 참고 자료

- [Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
- [Netpbm PPM format](https://netpbm.sourceforge.net/doc/ppm.html)
- [Microsoft: StretchDIBits](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits)
