# keyboard-piano-cpp

컴퓨터 키보드로 피아노와 드럼을 연주하는 Windows 콘솔 신시사이저다. Piano 모드는 키를 누르고 있는 시간과 여러 키의 동시 입력을 추적해 화음을 만들고, Drum Kit 모드는 여덟 개 타악기를 one-shot voice로 합성한다. 두 모드 모두 외부 음원 파일 없이 프로그램이 48 kHz PCM 파형을 실시간 계산한다.

FFmpeg은 필요하지 않다. 현재 프로그램은 소리를 재생만 하므로 Windows 기본 오디오 API인 WASAPI로 충분하다. 나중에 연주를 영상과 합치거나 AAC·MP3 같은 압축 파일로 내보낼 때는 FFmpeg가 유용하지만, 실시간 건반 입력과 소리 생성에는 관여하지 않는다.

## 프로젝트 정보

| 항목 | 내용 |
|---|---|
| 분야 | 실시간 오디오, 음악 소프트웨어, 절차적 악기 합성, 콘솔 입력 |
| 언어·표준 | C++23 |
| 플랫폼 | 64비트 Windows 10 이상, Cygwin GCC/G++ |
| 오디오 API | WASAPI 공유 모드, `IAudioClient3`, event-driven render |
| 입력 API | Win32 `GetAsyncKeyState`의 현재 key-down bit |
| 오디오 스레드 | MMCSS `Pro Audio`, `AVRT_PRIORITY_HIGH` |
| 동시 발음 수 | Piano release·sustain과 drum tail을 합쳐 최대 48 voice |
| Piano 음색 | Grand Piano, Electric Piano, Tonewheel Organ, Warm Pad |
| Drum Kit | Kick, Snare, Closed/Open Hat, Low/High Tom, Clap, Crash |
| 외부 라이브러리 | 없음 |
| 시스템 링크 | `ole32`, `user32`, `avrt` |
| 구현 난이도 | 오디오 3/5, 합성 이론 3/5, 전체 3/5 |

## 빌드와 실행

```sh
make run
```

오디오 장치 초기화만 검사하려면 다음 명령을 사용한다.

```sh
make check
```

현재 개발 장치에서 확인한 실제 출력은 다음과 같다.

```text
WASAPI IAudioClient3 ready: 48000 Hz, 128 frames, 2.67 ms period
```

`2.67 ms`는 WASAPI가 신호를 요청하는 엔진 주기이며 스피커까지의 전체 왕복 지연 측정값은 아니다. 장치의 mix format과 최소 주기가 다르면 실행 화면과 `make check` 결과도 달라진다.

## 연주 방법

`Tab`으로 `Piano`와 `Drum Kit`을 전환한다. 현재 모드와 전환 키, 해당 모드의 입력 배치는 콘솔 화면에 항상 표시된다.

### Piano

기본 옥타브는 C4부터 E5까지다. 검은 건반은 위쪽 문자 행에 배치했다.

```text
       W  E     T  Y  U     O  P
    A  S  D  F  G  H  J  K  L  ;
    C  D  E  F  G  A  B  C  D  E
```

| 입력 | 동작 |
|---|---|
| `Tab` | Piano와 Drum Kit 전환 |
| `A W S E D F T G Y H U J K O L P ;` | Piano 모드의 반음 단위 17개 건반 연주 |
| 여러 건반 동시 누름 | 화음 연주 |
| `Space` 누르고 있기 | Piano sustain pedal |
| `Z`, `X` | Piano 기준 옥타브 내림·올림, 범위 1~7 |
| `1`~`4` | Piano의 Grand, Electric, Organ, Warm Pad 선택 |
| `Up`, `Down` | master volume 5% 단위 조절 |
| `R` | Piano·drum voice와 reverb tail 즉시 제거 |
| `Esc` | 종료 |

### Drum Kit

| 키 | 타악기 | 합성 특징 |
|---|---|---|
| `A` | Kick | 빠르게 내려가는 저주파 body와 짧은 click |
| `S` | Snare | 두 북피 공진과 고역 잡음 wire |
| `D` | Closed Hat | 매우 짧은 비정수 금속 공진과 고역 잡음 |
| `F` | Open Hat | 긴 감쇠를 가진 hi-hat wash |
| `G` | Low Tom | 낮은 pitch drop과 두 개의 공진 성분 |
| `H` | High Tom | 더 높고 짧은 pitch drop |
| `J` | Clap | 시간차를 둔 세 번의 잡음 burst와 tail |
| `K` | Crash | 일곱 비정수 고주파 공진과 긴 noise tail |

Drum Kit은 key-down 순간에만 타격을 시작하고 키를 떼어도 악기별 tail이 자연스럽게 끝날 때까지 재생한다. 같은 패드를 빠르게 다시 치면 별도 voice가 겹친다. Closed/Open Hat을 새로 치면 이미 울리는 hi-hat을 35ms 동안 빠르게 감쇠시키는 choke가 적용된다.

키보드 하드웨어가 특정 조합을 동시에 보고하지 못하는 ghosting 또는 key rollover 제한은 프로그램이 복구할 수 없다. 화음 일부가 빠진다면 다른 키 조합이나 N-key rollover 키보드를 사용해야 한다.

## 오디오 처리 구조

### 1. 낮은 지연의 WASAPI 스트림

오디오 스레드가 기본 render endpoint에서 `IAudioClient3`를 활성화하고 장치의 mix format을 조회한다. `GetSharedModeEnginePeriod`가 보고한 최소 허용 주기를 `InitializeSharedAudioStream`에 전달하며, event가 도착할 때마다 다음 순서로 빈 buffer를 채운다.

```text
WASAPI event
  -> GetCurrentPadding
  -> IAudioRenderClient::GetBuffer
  -> voice 합성 + reverb + limiter
  -> IAudioRenderClient::ReleaseBuffer
```

공유 모드이므로 다른 프로그램의 소리와 함께 재생된다. 렌더 스레드는 MMCSS의 `Pro Audio` 작업으로 등록해 짧은 주기의 buffer underrun 가능성을 줄인다.

### 2. 음높이

MIDI note number `n`의 주파수는 12평균율 식을 사용한다.

```text
f(n) = 440 * 2^((n - 69) / 12) Hz
```

따라서 A4인 MIDI 69가 440 Hz이고 인접한 키는 주파수가 `2^(1/12)`배씩 변한다. 옥타브가 하나 올라가면 주파수는 두 배가 된다.

### 3. 다성 voice와 envelope

Piano 건반마다 독립적인 위상, 음높이, velocity, 음색, 경과 시간과 envelope 상태를 가진 voice를 할당한다. envelope는 `attack -> decay -> sustain -> release` 상태로 진행한다. 건반을 떼었을 때 sustain이 켜져 있으면 release를 미루고, 페달을 놓는 순간 더 이상 눌려 있지 않은 voice를 release로 전환한다.

48개 voice가 모두 사용 중이면 소리가 작고 이미 release 중인 voice를 우선 교체한다. 각 note key가 처음 눌렸을 때의 MIDI 번호를 따로 저장하므로, 음을 누른 채 옥타브를 바꿔도 기존 음의 key-up 처리가 어긋나지 않는다.

### 4. 음색 합성

- **Grand Piano**: 여덟 부분음의 additive synthesis, 현 강성에 따른 작은 inharmonicity, 빠르게 사라지는 hammer noise와 부분음별 감쇠를 사용한다.
- **Electric Piano**: 시간에 따라 약해지는 phase modulation과 bell 성분을 섞는다.
- **Tonewheel Organ**: 정수배 drawbar 계열 부분음을 지속적으로 합성한다.
- **Warm Pad**: 느린 attack·release와 낮은 차수 부분음으로 부드러운 지속음을 만든다.

이는 녹음된 실제 피아노 sample을 재생하는 방식이 아니다. 악기별 물리 모델을 완전히 재현하지도 않으며, 계산량과 외부 자산 없이 건반 반응성과 서로 구분되는 음색을 제공하는 절차적 합성이다.

### 5. Drum Kit 합성

Drum voice는 건반처럼 key-up을 기다리지 않고 타격 시점부터 악기별 수명이 끝날 때까지 진행하는 one-shot이다.

- **Kick·Tom**: 사인파의 주파수를 타격 직후 빠르게 낮추는 pitch envelope로 북피가 느슨해지는 인상을 만든다.
- **Snare**: 178Hz 부근 body와 326Hz 공진에 high-pass noise를 섞어 북피와 철망을 분리한다.
- **Hi-hat·Crash**: 정수배가 아닌 여러 고주파 oscillator와 filtered noise를 합쳐 복잡한 금속 스펙트럼을 근사한다.
- **Clap**: 27ms 간격의 세 noise burst와 더 느린 tail로 여러 손바닥 반사가 겹치는 모양을 만든다.

Piano와 drum은 별도 mixer가 아니라 같은 48개 voice pool과 stereo 합산 경로를 공유한다. 각 drum은 실제 kit의 위치처럼 좌우 pan이 다르며 hi-hat끼리는 choke group을 구성한다. 녹음 sample이나 완전한 membrane·금속판 물리 모델이 아니라 적은 자산으로 즉시 반응하는 절차적 근사다.

### 6. 공간감과 출력 보호

네 개의 feedback comb filter와 두 개의 all-pass filter를 좌우 길이가 조금 다르게 구성한 Schroeder 계열 stereo reverb를 사용한다. dry signal과 reverb tail을 섞은 뒤 `tanh` soft limiter로 큰 화음의 peak를 제한한다. 출력 장치가 float32, PCM 16·24·32 bit 중 하나이면 해당 형식으로 변환한다.

## 주요 파일

| 파일 | 책임 |
|---|---|
| [src/main.cpp](src/main.cpp) | 수명 관리, 모드별 콘솔 UI, Piano 건반·Drum pad 연결 |
| [src/audio_engine.cpp](src/audio_engine.cpp) | WASAPI, Piano·drum voice, 합성식, envelope, reverb, limiter |
| [src/keyboard.cpp](src/keyboard.cpp) | virtual-key polling, 눌림·해제 edge와 Piano 건반 배열 |
| [include/audio_engine.hpp](include/audio_engine.hpp) | 오디오 엔진 공개 API와 Piano·drum 종류 열거형 |
| [include/keyboard.hpp](include/keyboard.hpp) | 건반 mapping과 입력 상태 API |
| [Makefile](Makefile) | C++23 빌드, Windows import library 링크, 오디오 검사 |

프로젝트 전용 작업 규칙은 현재 필요하지 않아 별도 `AGENTS.md`를 두지 않으며 루트 규칙을 그대로 따른다.

## 현재 한계

- sample library, velocity-sensitive keyboard와 MIDI 입력이 없어 실제 grand piano의 음색과 타건 강도를 재현하지 않는다.
- 절차적 drum은 실제 drum kit 녹음보다 타격 질감과 cymbal의 불규칙한 modal vibration이 단순하다.
- `GetAsyncKeyState` polling 방식이라 프로그램이 실행 중일 때 다른 창에서 누른 키도 감지할 수 있다.
- 기본 오디오 endpoint가 실행 중 교체되거나 invalidation되면 자동으로 장치를 다시 여는 복구 경로가 없다.
- 최소 WASAPI engine period를 사용하므로 아주 느린 CPU나 불안정한 오디오 driver에서는 default period보다 glitch에 민감할 수 있다.
- 연주 녹음, metronome, 악보, MIDI, WAV export와 설정 저장은 아직 없다.
- 합성 parameter와 reverb tuning은 청감 기반 값이며 실제 악기 측정치로 calibration하지 않았다.

## 개선 방향

1. Windows MIDI Services 또는 MIDI 2.0 장치 입력과 velocity·pedal 연결
2. default endpoint 변경 감지와 끊김 없는 WASAPI stream 재생성
3. 선택 가능한 minimum/default period와 underrun 계측
4. WAV 녹음 및 선택적 FLAC·AAC·MP3 변환 단계 추가
5. SoundFont/SFZ sample engine 또는 현·soundboard physical modeling 음색 추가
6. drum pattern sequencer, metronome, tempo·swing과 loop recording
7. key transpose, scale lock, chord display와 연주 기록
8. exclusive mode 또는 ASIO backend의 선택적 저지연 경로 비교

압축 export를 구현할 때도 먼저 무손실 WAV를 직접 기록할 수 있다. FFmpeg는 그 WAV를 다른 codec이나 영상 container로 변환하는 선택적 후처리 도구로 추가하는 것이 적절하다.

## 참고 자료

- [Microsoft: IAudioClient3](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nn-audioclient-iaudioclient3)
- [Microsoft: IAudioClient3::InitializeSharedAudioStream](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient3-initializesharedaudiostream)
- [Microsoft: Low Latency Audio](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio)
- [Microsoft: Multimedia Class Scheduler Service](https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service)
- [Microsoft: GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)
