#include "audio_engine.hpp"

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace piano {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double tau = 2.0 * pi;
constexpr std::size_t max_voices = 48;
constexpr std::size_t harmonic_count = 8;

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() {
        reset();
    }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    [[nodiscard]] T* get() const noexcept {
        return pointer_;
    }

    [[nodiscard]] T** put() noexcept {
        reset();
        return &pointer_;
    }

    [[nodiscard]] T* operator->() const noexcept {
        return pointer_;
    }

    void reset() noexcept {
        if (pointer_ != nullptr) {
            pointer_->Release();
            pointer_ = nullptr;
        }
    }

private:
    T* pointer_ = nullptr;
};

class ScopedHandle {
public:
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~ScopedHandle() {
        reset();
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept {
        return handle_;
    }

    void reset(HANDLE handle = nullptr) noexcept {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = nullptr;
};

class CombFilter {
public:
    void configure(std::size_t length) {
        buffer_.assign(std::max<std::size_t>(length, 1), 0.0F);
        index_ = 0;
        filtered_ = 0.0F;
    }

    [[nodiscard]] float process(float input) noexcept {
        const float output = buffer_[index_];
        filtered_ = output * 0.72F + filtered_ * 0.28F;
        buffer_[index_] = input + filtered_ * 0.79F;
        index_ = (index_ + 1) % buffer_.size();
        return output;
    }

    void clear() noexcept {
        std::fill(buffer_.begin(), buffer_.end(), 0.0F);
        index_ = 0;
        filtered_ = 0.0F;
    }

private:
    std::vector<float> buffer_{1, 0.0F};
    std::size_t index_ = 0;
    float filtered_ = 0.0F;
};

class AllPassFilter {
public:
    void configure(std::size_t length) {
        buffer_.assign(std::max<std::size_t>(length, 1), 0.0F);
        index_ = 0;
    }

    [[nodiscard]] float process(float input) noexcept {
        const float delayed = buffer_[index_];
        const float output = delayed - input;
        buffer_[index_] = input + delayed * 0.5F;
        index_ = (index_ + 1) % buffer_.size();
        return output;
    }

    void clear() noexcept {
        std::fill(buffer_.begin(), buffer_.end(), 0.0F);
        index_ = 0;
    }

private:
    std::vector<float> buffer_{1, 0.0F};
    std::size_t index_ = 0;
};

class StereoReverb {
public:
    void configure(unsigned sample_rate) {
        constexpr std::array<unsigned, 4> comb_lengths{1116, 1188, 1277, 1356};
        constexpr std::array<unsigned, 2> allpass_lengths{556, 441};
        const double scale = static_cast<double>(sample_rate) / 44100.0;
        const auto scaled = [scale](unsigned value) {
            return static_cast<std::size_t>(std::lround(static_cast<double>(value) * scale));
        };

        for (std::size_t i = 0; i < comb_left_.size(); ++i) {
            comb_left_[i].configure(scaled(comb_lengths[i]));
            comb_right_[i].configure(scaled(comb_lengths[i] + 23));
        }
        for (std::size_t i = 0; i < allpass_left_.size(); ++i) {
            allpass_left_[i].configure(scaled(allpass_lengths[i]));
            allpass_right_[i].configure(scaled(allpass_lengths[i] + 23));
        }
    }

    [[nodiscard]] std::pair<float, float> process(float left, float right) noexcept {
        const float input_left = left * 0.16F + right * 0.04F;
        const float input_right = right * 0.16F + left * 0.04F;
        float wet_left = 0.0F;
        float wet_right = 0.0F;

        for (std::size_t i = 0; i < comb_left_.size(); ++i) {
            wet_left += comb_left_[i].process(input_left);
            wet_right += comb_right_[i].process(input_right);
        }
        wet_left *= 0.25F;
        wet_right *= 0.25F;

        for (std::size_t i = 0; i < allpass_left_.size(); ++i) {
            wet_left = allpass_left_[i].process(wet_left);
            wet_right = allpass_right_[i].process(wet_right);
        }
        return {wet_left, wet_right};
    }

    void clear() noexcept {
        for (auto& comb : comb_left_) {
            comb.clear();
        }
        for (auto& comb : comb_right_) {
            comb.clear();
        }
        for (auto& allpass : allpass_left_) {
            allpass.clear();
        }
        for (auto& allpass : allpass_right_) {
            allpass.clear();
        }
    }

private:
    std::array<CombFilter, 4> comb_left_;
    std::array<CombFilter, 4> comb_right_;
    std::array<AllPassFilter, 2> allpass_left_;
    std::array<AllPassFilter, 2> allpass_right_;
};

enum class EnvelopeStage {
    off,
    attack,
    decay,
    sustain,
    release,
};

enum class VoiceKind {
    melodic,
    drum,
};

struct EnvelopeParameters {
    float attack_seconds;
    float decay_seconds;
    float sustain_level;
    float release_seconds;
};

struct Voice {
    VoiceKind kind = VoiceKind::melodic;
    EnvelopeStage stage = EnvelopeStage::off;
    Timbre timbre = Timbre::grand_piano;
    DrumSound drum = DrumSound::kick;
    int midi_note = 60;
    float velocity = 0.85F;
    float envelope = 0.0F;
    float release_rate = 0.0F;
    bool key_down = false;
    int held_count = 0;
    double age_seconds = 0.0;
    double choke_started_seconds = -1.0;
    std::array<double, harmonic_count> phases{};
    std::uint32_t noise_state = 1;
    float filter_state = 0.0F;
};

[[nodiscard]] EnvelopeParameters envelope_for(Timbre timbre) noexcept {
    switch (timbre) {
        case Timbre::grand_piano:
            return {0.004F, 0.70F, 0.34F, 0.65F};
        case Timbre::electric_piano:
            return {0.008F, 0.95F, 0.48F, 0.90F};
        case Timbre::tonewheel_organ:
            return {0.012F, 0.08F, 0.94F, 0.18F};
        case Timbre::warm_pad:
            return {0.16F, 1.10F, 0.76F, 1.80F};
    }
    return {0.01F, 0.5F, 0.5F, 0.5F};
}

[[nodiscard]] double midi_frequency(int midi_note) noexcept {
    return 440.0 * std::exp2((static_cast<double>(midi_note) - 69.0) / 12.0);
}

void begin_release(Voice& voice) noexcept {
    if (voice.stage == EnvelopeStage::off || voice.stage == EnvelopeStage::release) {
        return;
    }
    const float release_seconds = envelope_for(voice.timbre).release_seconds;
    voice.release_rate = voice.envelope / std::max(release_seconds, 0.001F);
    voice.stage = EnvelopeStage::release;
}

void advance_envelope(Voice& voice, float delta_seconds) noexcept {
    const EnvelopeParameters parameters = envelope_for(voice.timbre);
    switch (voice.stage) {
        case EnvelopeStage::off:
            break;
        case EnvelopeStage::attack:
            voice.envelope += delta_seconds / parameters.attack_seconds;
            if (voice.envelope >= 1.0F) {
                voice.envelope = 1.0F;
                voice.stage = EnvelopeStage::decay;
            }
            break;
        case EnvelopeStage::decay:
            voice.envelope -= delta_seconds * (1.0F - parameters.sustain_level) /
                              parameters.decay_seconds;
            if (voice.envelope <= parameters.sustain_level) {
                voice.envelope = parameters.sustain_level;
                voice.stage = EnvelopeStage::sustain;
            }
            break;
        case EnvelopeStage::sustain:
            voice.envelope = parameters.sustain_level;
            break;
        case EnvelopeStage::release:
            voice.envelope -= voice.release_rate * delta_seconds;
            if (voice.envelope <= 0.0001F) {
                voice.envelope = 0.0F;
                voice.stage = EnvelopeStage::off;
                voice.key_down = false;
            }
            break;
    }
}

[[nodiscard]] float next_noise(Voice& voice) noexcept {
    voice.noise_state ^= voice.noise_state << 13U;
    voice.noise_state ^= voice.noise_state >> 17U;
    voice.noise_state ^= voice.noise_state << 5U;
    constexpr float scale = 1.0F / static_cast<float>(std::numeric_limits<std::uint32_t>::max());
    return static_cast<float>(voice.noise_state) * scale * 2.0F - 1.0F;
}

[[nodiscard]] float highpass_noise(Voice& voice, float noise, float smoothing) noexcept {
    voice.filter_state += smoothing * (noise - voice.filter_state);
    return noise - voice.filter_state;
}

void advance_phase(Voice& voice, std::size_t index, double frequency,
                   double sample_rate) noexcept {
    voice.phases[index] += tau * frequency / sample_rate;
    if (voice.phases[index] >= tau) {
        voice.phases[index] = std::fmod(voice.phases[index], tau);
    }
}

[[nodiscard]] float render_oscillator(Voice& voice, double sample_rate) noexcept {
    const double frequency = midi_frequency(voice.midi_note);
    const double nyquist_limit = sample_rate * 0.45;
    float sample = 0.0F;

    switch (voice.timbre) {
        case Timbre::grand_piano: {
            constexpr std::array<float, harmonic_count> amplitudes{
                1.0F, 0.48F, 0.28F, 0.16F, 0.10F, 0.065F, 0.040F, 0.025F};
            for (std::size_t i = 0; i < harmonic_count; ++i) {
                const double harmonic = static_cast<double>(i + 1);
                if (frequency * harmonic >= nyquist_limit) {
                    break;
                }
                const double decay = std::exp(-voice.age_seconds * (0.32 + harmonic * 0.16));
                sample += amplitudes[i] * static_cast<float>(decay * std::sin(voice.phases[i]));
            }
            const float hammer = next_noise(voice) *
                                 static_cast<float>(0.09 * std::exp(-voice.age_seconds * 48.0));
            sample = sample * 0.60F + hammer;
            break;
        }
        case Timbre::electric_piano: {
            const double modulation = 1.75 * std::exp(-voice.age_seconds * 2.8) *
                                      std::sin(voice.phases[3]);
            sample = static_cast<float>(0.72 * std::sin(voice.phases[0] + modulation) +
                                        0.20 * std::exp(-voice.age_seconds * 1.1) *
                                            std::sin(voice.phases[1]) +
                                        0.08 * std::sin(voice.phases[2]));
            break;
        }
        case Timbre::tonewheel_organ:
            sample = static_cast<float>(0.52 * std::sin(voice.phases[0]) +
                                        0.22 * std::sin(voice.phases[1]) +
                                        0.12 * std::sin(voice.phases[2]) +
                                        0.09 * std::sin(voice.phases[3]) +
                                        0.05 * std::sin(voice.phases[5]));
            break;
        case Timbre::warm_pad:
            sample = static_cast<float>(0.70 * std::sin(voice.phases[0]) +
                                        0.19 * std::sin(voice.phases[1] + 0.4) +
                                        0.08 * std::sin(voice.phases[2] + 1.1) +
                                        0.03 * std::sin(voice.phases[3] + 1.8));
            break;
    }

    const double stiffness = voice.timbre == Timbre::grand_piano
                                 ? 0.000055 + std::max(voice.midi_note - 60, 0) * 0.0000015
                                 : 0.0;
    for (std::size_t i = 0; i < harmonic_count; ++i) {
        const double harmonic = static_cast<double>(i + 1);
        const double ratio = harmonic * std::sqrt(1.0 + stiffness * harmonic * harmonic);
        voice.phases[i] += tau * frequency * ratio / sample_rate;
        if (voice.phases[i] >= tau) {
            voice.phases[i] = std::fmod(voice.phases[i], tau);
        }
    }
    voice.age_seconds += 1.0 / sample_rate;
    return sample;
}

[[nodiscard]] double drum_duration(DrumSound drum) noexcept {
    switch (drum) {
        case DrumSound::kick:
            return 1.25;
        case DrumSound::snare:
            return 0.90;
        case DrumSound::closed_hat:
            return 0.24;
        case DrumSound::open_hat:
            return 1.80;
        case DrumSound::low_tom:
            return 1.45;
        case DrumSound::high_tom:
            return 1.15;
        case DrumSound::clap:
            return 0.75;
        case DrumSound::crash:
            return 4.50;
    }
    return 1.0;
}

[[nodiscard]] float render_drum(Voice& voice, double sample_rate) noexcept {
    const double time = voice.age_seconds;
    const double nyquist_limit = sample_rate * 0.45;
    float sample = 0.0F;
    float amplitude = 0.0F;

    switch (voice.drum) {
        case DrumSound::kick: {
            const double frequency = 48.0 + 118.0 * std::exp(-time * 32.0);
            advance_phase(voice, 0, frequency, sample_rate);
            advance_phase(voice, 1, frequency * 2.01, sample_rate);
            amplitude = static_cast<float>((1.0 - std::exp(-time * 600.0)) *
                                           std::exp(-time * 6.4));
            const float body = static_cast<float>(std::sin(voice.phases[0]) +
                                                  0.16 * std::sin(voice.phases[1]));
            const float click = highpass_noise(voice, next_noise(voice), 0.10F) *
                                static_cast<float>(0.22 * std::exp(-time * 75.0));
            sample = body * amplitude * 0.95F + click;
            break;
        }
        case DrumSound::snare: {
            const double body_frequency = 178.0 + 34.0 * std::exp(-time * 24.0);
            advance_phase(voice, 0, body_frequency, sample_rate);
            advance_phase(voice, 1, 326.0, sample_rate);
            const float body_envelope = static_cast<float>(std::exp(-time * 10.5));
            const float noise_envelope = static_cast<float>(std::exp(-time * 15.5));
            const float body = static_cast<float>(0.72 * std::sin(voice.phases[0]) +
                                                  0.28 * std::sin(voice.phases[1]));
            const float wires = highpass_noise(voice, next_noise(voice), 0.075F);
            amplitude = std::max(body_envelope, noise_envelope);
            sample = body * body_envelope * 0.44F + wires * noise_envelope * 0.72F;
            break;
        }
        case DrumSound::closed_hat:
        case DrumSound::open_hat: {
            constexpr std::array<double, 6> frequencies{
                5420.0, 7310.0, 8870.0, 10640.0, 12830.0, 14920.0};
            float metal = 0.0F;
            int components = 0;
            for (std::size_t i = 0; i < frequencies.size(); ++i) {
                if (frequencies[i] >= nyquist_limit) {
                    continue;
                }
                advance_phase(voice, i, frequencies[i], sample_rate);
                metal += static_cast<float>(std::sin(voice.phases[i]));
                ++components;
            }
            if (components > 0) {
                metal /= static_cast<float>(components);
            }
            const double decay = voice.drum == DrumSound::closed_hat ? 34.0 : 4.8;
            amplitude = static_cast<float>((1.0 - std::exp(-time * 1200.0)) *
                                           std::exp(-time * decay));
            const float hiss = highpass_noise(voice, next_noise(voice), 0.16F);
            sample = (metal * 0.58F + hiss * 0.55F) * amplitude;
            break;
        }
        case DrumSound::low_tom:
        case DrumSound::high_tom: {
            const bool high = voice.drum == DrumSound::high_tom;
            const double base = high ? 170.0 : 102.0;
            const double drop = high ? 82.0 : 58.0;
            const double frequency = base + drop * std::exp(-time * 19.0);
            advance_phase(voice, 0, frequency, sample_rate);
            advance_phase(voice, 1, frequency * 1.51, sample_rate);
            amplitude = static_cast<float>((1.0 - std::exp(-time * 700.0)) *
                                           std::exp(-time * (high ? 7.0 : 5.4)));
            const float strike = highpass_noise(voice, next_noise(voice), 0.10F) *
                                 static_cast<float>(0.12 * std::exp(-time * 60.0));
            sample = static_cast<float>(std::sin(voice.phases[0]) * 0.88 +
                                        std::sin(voice.phases[1]) * 0.12) * amplitude + strike;
            break;
        }
        case DrumSound::clap: {
            const auto burst = [time](double start, double decay) {
                return time >= start ? std::exp(-(time - start) * decay) : 0.0;
            };
            const double early = burst(0.0, 92.0) + 0.88 * burst(0.027, 96.0) +
                                 0.72 * burst(0.054, 104.0);
            const double tail = 0.32 * std::exp(-time * 10.5);
            amplitude = static_cast<float>(std::min(early + tail, 1.35));
            const float noise = highpass_noise(voice, next_noise(voice), 0.055F);
            sample = noise * amplitude * 0.78F;
            break;
        }
        case DrumSound::crash: {
            constexpr std::array<double, 7> frequencies{
                2870.0, 4210.0, 5630.0, 7480.0, 9310.0, 11740.0, 13960.0};
            float metal = 0.0F;
            int components = 0;
            for (std::size_t i = 0; i < frequencies.size(); ++i) {
                if (frequencies[i] >= nyquist_limit) {
                    continue;
                }
                advance_phase(voice, i, frequencies[i], sample_rate);
                const float weight = 1.0F / std::sqrt(static_cast<float>(i + 1));
                metal += static_cast<float>(std::sin(voice.phases[i])) * weight;
                ++components;
            }
            if (components > 0) {
                metal /= std::sqrt(static_cast<float>(components));
            }
            amplitude = static_cast<float>((1.0 - std::exp(-time * 900.0)) *
                                           (0.78 * std::exp(-time * 0.95) +
                                            0.22 * std::exp(-time * 3.8)));
            const float wash = highpass_noise(voice, next_noise(voice), 0.11F);
            sample = (metal * 0.58F + wash * 0.35F) * amplitude;
            break;
        }
    }

    bool choke_finished = false;
    if (voice.choke_started_seconds >= 0.0) {
        const double choke_age = time - voice.choke_started_seconds;
        const float choke_gain = static_cast<float>(std::clamp(1.0 - choke_age / 0.035,
                                                                0.0, 1.0));
        sample *= choke_gain;
        amplitude *= choke_gain;
        choke_finished = choke_age >= 0.035;
    }

    voice.envelope = amplitude;
    voice.age_seconds += 1.0 / sample_rate;
    if (choke_finished || voice.age_seconds >= drum_duration(voice.drum) ||
        (voice.age_seconds > 0.01 && amplitude < 0.00005F)) {
        voice.stage = EnvelopeStage::off;
        voice.envelope = 0.0F;
    }
    return sample;
}

[[nodiscard]] float drum_pan(DrumSound drum) noexcept {
    switch (drum) {
        case DrumSound::kick:
            return 0.0F;
        case DrumSound::snare:
            return -0.05F;
        case DrumSound::closed_hat:
            return 0.48F;
        case DrumSound::open_hat:
            return 0.58F;
        case DrumSound::low_tom:
            return -0.42F;
        case DrumSound::high_tom:
            return 0.28F;
        case DrumSound::clap:
            return 0.10F;
        case DrumSound::crash:
            return -0.58F;
    }
    return 0.0F;
}

[[nodiscard]] float soft_limit(float sample) noexcept {
    return std::tanh(sample * 1.15F);
}

enum class SampleEncoding {
    float32,
    pcm16,
    pcm24,
    pcm32,
    unsupported,
};

[[nodiscard]] SampleEncoding detect_encoding(const WAVEFORMATEX& format) noexcept {
    WORD format_tag = format.wFormatTag;
    if (format_tag == WAVE_FORMAT_EXTENSIBLE &&
        format.cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        const auto& extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
        format_tag = static_cast<WORD>(extended.SubFormat.Data1);
    }

    if (format_tag == WAVE_FORMAT_IEEE_FLOAT && format.wBitsPerSample == 32) {
        return SampleEncoding::float32;
    }
    if (format_tag == WAVE_FORMAT_PCM && format.wBitsPerSample == 16) {
        return SampleEncoding::pcm16;
    }
    if (format_tag == WAVE_FORMAT_PCM && format.wBitsPerSample == 24) {
        return SampleEncoding::pcm24;
    }
    if (format_tag == WAVE_FORMAT_PCM && format.wBitsPerSample == 32) {
        return SampleEncoding::pcm32;
    }
    return SampleEncoding::unsupported;
}

[[nodiscard]] std::string hresult_error(std::string_view operation, HRESULT result) {
    std::ostringstream stream;
    stream << operation << " failed (HRESULT 0x" << std::uppercase << std::hex
           << static_cast<std::uint32_t>(result) << ')';
    return stream.str();
}

}  // namespace

struct AudioEngine::Impl {
    mutable std::mutex synth_mutex;
    std::array<Voice, max_voices> voices{};
    StereoReverb reverb;
    bool sustain_enabled = false;
    std::uint32_t trigger_sequence = 0;

    std::atomic<Timbre> selected_timbre{Timbre::grand_piano};
    std::atomic<float> volume{0.68F};
    std::atomic<unsigned> stream_sample_rate{0};
    std::atomic<unsigned> stream_period_frames{0};
    std::atomic<bool> stream_healthy{false};

    ScopedHandle stop_event;
    std::thread render_thread;
    std::mutex startup_mutex;
    std::condition_variable startup_condition;
    bool startup_done = false;
    bool startup_succeeded = false;
    std::string startup_error;

    [[nodiscard]] std::pair<float, float> synthesize_frame(double sample_rate) noexcept {
        float left = 0.0F;
        float right = 0.0F;
        const float delta_seconds = 1.0F / static_cast<float>(sample_rate);

        for (Voice& voice : voices) {
            if (voice.stage == EnvelopeStage::off) {
                continue;
            }
            float raw = 0.0F;
            float pan = 0.0F;
            if (voice.kind == VoiceKind::drum) {
                raw = render_drum(voice, sample_rate) * voice.velocity;
                pan = drum_pan(voice.drum);
            } else {
                advance_envelope(voice, delta_seconds);
                if (voice.stage == EnvelopeStage::off) {
                    continue;
                }
                raw = render_oscillator(voice, sample_rate) * voice.envelope * voice.velocity;
                pan = std::clamp((static_cast<float>(voice.midi_note) - 66.0F) / 42.0F,
                                 -0.70F, 0.70F);
            }
            const float angle = (pan + 1.0F) * static_cast<float>(pi) * 0.25F;
            left += raw * std::cos(angle);
            right += raw * std::sin(angle);
        }

        const float master = volume.load(std::memory_order_relaxed) * 0.30F;
        left *= master;
        right *= master;
        const auto [wet_left, wet_right] = reverb.process(left, right);
        return {soft_limit(left * 0.86F + wet_left * 0.28F),
                soft_limit(right * 0.86F + wet_right * 0.28F)};
    }

    void render_frames(BYTE* destination, UINT32 frame_count, const WAVEFORMATEX& format,
                       SampleEncoding encoding) noexcept {
        std::memset(destination, 0,
                    static_cast<std::size_t>(frame_count) * format.nBlockAlign);
        std::scoped_lock lock(synth_mutex);
        const unsigned channels = format.nChannels;
        const double rate = static_cast<double>(format.nSamplesPerSec);

        for (UINT32 frame_index = 0; frame_index < frame_count; ++frame_index) {
            const auto [left, right] = synthesize_frame(rate);
            const float mono = (left + right) * 0.5F;
            BYTE* frame = destination + static_cast<std::size_t>(frame_index) * format.nBlockAlign;

            switch (encoding) {
                case SampleEncoding::float32: {
                    auto* samples = reinterpret_cast<float*>(frame);
                    samples[0] = channels == 1 ? mono : left;
                    if (channels >= 2) {
                        samples[1] = right;
                    }
                    break;
                }
                case SampleEncoding::pcm16: {
                    auto* samples = reinterpret_cast<std::int16_t*>(frame);
                    samples[0] = static_cast<std::int16_t>(std::lround(
                        std::clamp(channels == 1 ? mono : left, -1.0F, 1.0F) * 32767.0F));
                    if (channels >= 2) {
                        samples[1] = static_cast<std::int16_t>(std::lround(
                            std::clamp(right, -1.0F, 1.0F) * 32767.0F));
                    }
                    break;
                }
                case SampleEncoding::pcm24: {
                    const auto write_sample = [](BYTE* output, float value) {
                        const auto sample = static_cast<std::int32_t>(std::lround(
                            std::clamp(value, -1.0F, 1.0F) * 8388607.0F));
                        output[0] = static_cast<BYTE>(sample & 0xFF);
                        output[1] = static_cast<BYTE>((sample >> 8) & 0xFF);
                        output[2] = static_cast<BYTE>((sample >> 16) & 0xFF);
                    };
                    write_sample(frame, channels == 1 ? mono : left);
                    if (channels >= 2) {
                        write_sample(frame + 3, right);
                    }
                    break;
                }
                case SampleEncoding::pcm32: {
                    auto* samples = reinterpret_cast<std::int32_t*>(frame);
                    constexpr double maximum = 2147483647.0;
                    samples[0] = static_cast<std::int32_t>(std::llround(
                        static_cast<double>(std::clamp(channels == 1 ? mono : left, -1.0F, 1.0F)) *
                        maximum));
                    if (channels >= 2) {
                        samples[1] = static_cast<std::int32_t>(std::llround(
                            static_cast<double>(std::clamp(right, -1.0F, 1.0F)) * maximum));
                    }
                    break;
                }
                case SampleEncoding::unsupported:
                    break;
            }
        }
    }

    void complete_startup(bool succeeded, std::string error = {}) {
        {
            std::scoped_lock lock(startup_mutex);
            if (startup_done) {
                return;
            }
            startup_succeeded = succeeded;
            startup_error = std::move(error);
            startup_done = true;
        }
        startup_condition.notify_one();
    }

    void audio_thread_body() {
        const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(com_result)) {
            complete_startup(false, hresult_error("CoInitializeEx", com_result));
            return;
        }

        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioClient3> client;
        ComPtr<IAudioRenderClient> render_client;
        ScopedHandle audio_event;
        WAVEFORMATEX* raw_format = nullptr;
        bool client_started = false;
        HANDLE mmcss_handle = nullptr;
        std::string error;
        HRESULT result = S_OK;
        UINT32 buffer_frames = 0;
        UINT32 default_period = 0;
        UINT32 fundamental_period = 0;
        UINT32 minimum_period = 0;
        UINT32 maximum_period = 0;
        SampleEncoding encoding = SampleEncoding::unsupported;

        do {
            result = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                      IID_IMMDeviceEnumerator,
                                      reinterpret_cast<void**>(enumerator.put()));
            if (FAILED(result)) {
                error = hresult_error("CoCreateInstance", result);
                break;
            }
            result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put());
            if (FAILED(result)) {
                error = hresult_error("GetDefaultAudioEndpoint", result);
                break;
            }
            result = device->Activate(IID_IAudioClient3, CLSCTX_ALL, nullptr,
                                      reinterpret_cast<void**>(client.put()));
            if (FAILED(result)) {
                error = hresult_error("Activate IAudioClient3", result);
                break;
            }
            result = client->GetMixFormat(&raw_format);
            if (FAILED(result)) {
                error = hresult_error("GetMixFormat", result);
                break;
            }
            encoding = detect_encoding(*raw_format);
            if (encoding == SampleEncoding::unsupported || raw_format->nChannels == 0) {
                error = "The default WASAPI mix format is not supported by this synthesizer";
                break;
            }
            result = client->GetSharedModeEnginePeriod(raw_format, &default_period,
                                                       &fundamental_period, &minimum_period,
                                                       &maximum_period);
            if (FAILED(result)) {
                error = hresult_error("GetSharedModeEnginePeriod", result);
                break;
            }
            result = client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                         minimum_period, raw_format, nullptr);
            if (FAILED(result)) {
                error = hresult_error("InitializeSharedAudioStream", result);
                break;
            }
            audio_event.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
            if (audio_event.get() == nullptr) {
                error = "CreateEventW failed for the WASAPI render event";
                break;
            }
            result = client->SetEventHandle(audio_event.get());
            if (FAILED(result)) {
                error = hresult_error("SetEventHandle", result);
                break;
            }
            result = client->GetBufferSize(&buffer_frames);
            if (FAILED(result)) {
                error = hresult_error("GetBufferSize", result);
                break;
            }
            result = client->GetService(IID_IAudioRenderClient,
                                        reinterpret_cast<void**>(render_client.put()));
            if (FAILED(result)) {
                error = hresult_error("GetService IAudioRenderClient", result);
                break;
            }

            {
                std::scoped_lock lock(synth_mutex);
                reverb.configure(raw_format->nSamplesPerSec);
            }

            BYTE* initial_buffer = nullptr;
            result = render_client->GetBuffer(buffer_frames, &initial_buffer);
            if (FAILED(result)) {
                error = hresult_error("GetBuffer", result);
                break;
            }
            render_frames(initial_buffer, buffer_frames, *raw_format, encoding);
            result = render_client->ReleaseBuffer(buffer_frames, 0);
            if (FAILED(result)) {
                error = hresult_error("ReleaseBuffer", result);
                break;
            }
            result = client->Start();
            if (FAILED(result)) {
                error = hresult_error("IAudioClient::Start", result);
                break;
            }
            client_started = true;
        } while (false);

        if (!client_started) {
            complete_startup(false, error);
            if (raw_format != nullptr) {
                CoTaskMemFree(raw_format);
            }
            render_client.reset();
            client.reset();
            device.reset();
            enumerator.reset();
            CoUninitialize();
            return;
        }

        stream_sample_rate.store(raw_format->nSamplesPerSec, std::memory_order_release);
        stream_period_frames.store(minimum_period, std::memory_order_release);
        stream_healthy.store(true, std::memory_order_release);
        complete_startup(true);

        DWORD task_index = 0;
        mmcss_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
        if (mmcss_handle != nullptr) {
            AvSetMmThreadPriority(mmcss_handle, AVRT_PRIORITY_HIGH);
        } else {
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        }

        const std::array<HANDLE, 2> wait_handles{stop_event.get(), audio_event.get()};
        while (stream_healthy.load(std::memory_order_acquire)) {
            const DWORD wait_result = WaitForMultipleObjects(
                static_cast<DWORD>(wait_handles.size()), wait_handles.data(), FALSE, 2000);
            if (wait_result == WAIT_OBJECT_0) {
                break;
            }
            if (wait_result == WAIT_TIMEOUT) {
                continue;
            }
            if (wait_result != WAIT_OBJECT_0 + 1) {
                stream_healthy.store(false, std::memory_order_release);
                break;
            }

            UINT32 padding = 0;
            result = client->GetCurrentPadding(&padding);
            if (FAILED(result)) {
                stream_healthy.store(false, std::memory_order_release);
                break;
            }
            const UINT32 available = buffer_frames - std::min(buffer_frames, padding);
            if (available == 0) {
                continue;
            }

            BYTE* output = nullptr;
            result = render_client->GetBuffer(available, &output);
            if (FAILED(result)) {
                stream_healthy.store(false, std::memory_order_release);
                break;
            }
            render_frames(output, available, *raw_format, encoding);
            result = render_client->ReleaseBuffer(available, 0);
            if (FAILED(result)) {
                stream_healthy.store(false, std::memory_order_release);
                break;
            }
        }

        client->Stop();
        stream_healthy.store(false, std::memory_order_release);
        if (mmcss_handle != nullptr) {
            AvRevertMmThreadCharacteristics(mmcss_handle);
        }
        CoTaskMemFree(raw_format);
        render_client.reset();
        client.reset();
        device.reset();
        enumerator.reset();
        CoUninitialize();
    }

    void audio_thread_main() noexcept {
        try {
            audio_thread_body();
        } catch (const std::exception& exception) {
            stream_healthy.store(false, std::memory_order_release);
            complete_startup(false, std::string("Audio thread error: ") + exception.what());
        } catch (...) {
            stream_healthy.store(false, std::memory_order_release);
            complete_startup(false, "Unknown audio thread error");
        }
    }
};

std::string_view timbre_name(Timbre timbre) noexcept {
    switch (timbre) {
        case Timbre::grand_piano:
            return "Grand Piano";
        case Timbre::electric_piano:
            return "Electric Piano";
        case Timbre::tonewheel_organ:
            return "Tonewheel Organ";
        case Timbre::warm_pad:
            return "Warm Pad";
    }
    return "Unknown";
}

std::string_view drum_name(DrumSound drum) noexcept {
    switch (drum) {
        case DrumSound::kick:
            return "Kick";
        case DrumSound::snare:
            return "Snare";
        case DrumSound::closed_hat:
            return "Closed Hat";
        case DrumSound::open_hat:
            return "Open Hat";
        case DrumSound::low_tom:
            return "Low Tom";
        case DrumSound::high_tom:
            return "High Tom";
        case DrumSound::clap:
            return "Clap";
        case DrumSound::crash:
            return "Crash";
    }
    return "Unknown";
}

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start(std::string& error) {
    if (impl_->render_thread.joinable()) {
        error.clear();
        return true;
    }

    {
        std::scoped_lock lock(impl_->startup_mutex);
        impl_->startup_done = false;
        impl_->startup_succeeded = false;
        impl_->startup_error.clear();
    }
    impl_->stop_event.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (impl_->stop_event.get() == nullptr) {
        error = "CreateEventW failed for the audio stop event";
        return false;
    }

    try {
        impl_->render_thread = std::thread(&Impl::audio_thread_main, impl_.get());
    } catch (const std::exception& exception) {
        impl_->stop_event.reset();
        error = std::string("Could not create audio thread: ") + exception.what();
        return false;
    }

    {
        std::unique_lock lock(impl_->startup_mutex);
        impl_->startup_condition.wait(lock, [this] { return impl_->startup_done; });
        error = impl_->startup_error;
        if (impl_->startup_succeeded) {
            return true;
        }
    }

    impl_->render_thread.join();
    impl_->stop_event.reset();
    return false;
}

void AudioEngine::stop() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->stop_event.get() != nullptr) {
        SetEvent(impl_->stop_event.get());
    }
    if (impl_->render_thread.joinable()) {
        impl_->render_thread.join();
    }
    impl_->stop_event.reset();
}

void AudioEngine::note_on(int midi_note, float velocity) {
    if (midi_note < 0 || midi_note > 127) {
        return;
    }
    std::scoped_lock lock(impl_->synth_mutex);

    Voice* selected = nullptr;
    for (Voice& voice : impl_->voices) {
        if (voice.kind == VoiceKind::melodic && voice.stage != EnvelopeStage::off &&
            voice.midi_note == midi_note) {
            if (voice.key_down) {
                ++voice.held_count;
                return;
            }
            selected = &voice;
            break;
        }
        if (selected == nullptr && voice.stage == EnvelopeStage::off) {
            selected = &voice;
        }
    }
    if (selected == nullptr) {
        selected = &*std::min_element(impl_->voices.begin(), impl_->voices.end(),
                                      [](const Voice& left, const Voice& right) {
                                          const float left_score = left.envelope *
                                              (left.key_down ? 2.0F : 1.0F);
                                          const float right_score = right.envelope *
                                              (right.key_down ? 2.0F : 1.0F);
                                          return left_score < right_score;
                                      });
    }

    *selected = Voice{};
    selected->kind = VoiceKind::melodic;
    selected->stage = EnvelopeStage::attack;
    selected->timbre = impl_->selected_timbre.load(std::memory_order_relaxed);
    selected->midi_note = midi_note;
    selected->velocity = std::clamp(velocity, 0.0F, 1.0F);
    selected->key_down = true;
    selected->held_count = 1;
    selected->noise_state = 0x9E3779B9U ^ static_cast<std::uint32_t>(midi_note * 2654435761U);
}

void AudioEngine::note_off(int midi_note) {
    std::scoped_lock lock(impl_->synth_mutex);
    for (Voice& voice : impl_->voices) {
        if (voice.kind != VoiceKind::melodic || voice.stage == EnvelopeStage::off ||
            voice.midi_note != midi_note) {
            continue;
        }
        if (voice.held_count > 0) {
            --voice.held_count;
        }
        if (voice.held_count > 0) {
            continue;
        }
        voice.key_down = false;
        if (!impl_->sustain_enabled) {
            begin_release(voice);
        }
    }
}

void AudioEngine::trigger_drum(DrumSound drum, float velocity) {
    std::scoped_lock lock(impl_->synth_mutex);
    if (drum == DrumSound::closed_hat || drum == DrumSound::open_hat) {
        for (Voice& voice : impl_->voices) {
            if (voice.kind == VoiceKind::drum && voice.stage != EnvelopeStage::off &&
                (voice.drum == DrumSound::closed_hat || voice.drum == DrumSound::open_hat)) {
                voice.choke_started_seconds = voice.age_seconds;
            }
        }
    }

    Voice* selected = nullptr;
    for (Voice& voice : impl_->voices) {
        if (voice.stage == EnvelopeStage::off) {
            selected = &voice;
            break;
        }
    }
    if (selected == nullptr) {
        selected = &*std::min_element(impl_->voices.begin(), impl_->voices.end(),
                                      [](const Voice& left, const Voice& right) {
                                          const float left_score = left.envelope *
                                              (left.key_down ? 2.0F : 1.0F);
                                          const float right_score = right.envelope *
                                              (right.key_down ? 2.0F : 1.0F);
                                          return left_score < right_score;
                                      });
    }

    *selected = Voice{};
    selected->kind = VoiceKind::drum;
    selected->stage = EnvelopeStage::sustain;
    selected->drum = drum;
    selected->velocity = std::clamp(velocity, 0.0F, 1.0F);
    selected->envelope = 1.0F;
    ++impl_->trigger_sequence;
    selected->noise_state = 0xA511E9B3U ^
        (static_cast<std::uint32_t>(drum) + 1U) * 0x9E3779B9U ^
        impl_->trigger_sequence * 0x85EBCA6BU;
}

void AudioEngine::all_notes_off() {
    std::scoped_lock lock(impl_->synth_mutex);
    impl_->voices = {};
    impl_->reverb.clear();
}

void AudioEngine::set_sustain(bool enabled) {
    std::scoped_lock lock(impl_->synth_mutex);
    if (impl_->sustain_enabled == enabled) {
        return;
    }
    impl_->sustain_enabled = enabled;
    if (!enabled) {
        for (Voice& voice : impl_->voices) {
            if (voice.kind == VoiceKind::melodic && !voice.key_down) {
                begin_release(voice);
            }
        }
    }
}

bool AudioEngine::sustain() const {
    std::scoped_lock lock(impl_->synth_mutex);
    return impl_->sustain_enabled;
}

void AudioEngine::set_timbre(Timbre timbre) noexcept {
    impl_->selected_timbre.store(timbre, std::memory_order_release);
}

Timbre AudioEngine::timbre() const noexcept {
    return impl_->selected_timbre.load(std::memory_order_acquire);
}

void AudioEngine::set_master_volume(float volume) noexcept {
    impl_->volume.store(std::clamp(volume, 0.0F, 1.0F), std::memory_order_release);
}

float AudioEngine::master_volume() const noexcept {
    return impl_->volume.load(std::memory_order_acquire);
}

int AudioEngine::active_voice_count() const {
    std::scoped_lock lock(impl_->synth_mutex);
    return static_cast<int>(std::count_if(impl_->voices.begin(), impl_->voices.end(),
                                          [](const Voice& voice) {
                                              return voice.stage != EnvelopeStage::off;
                                          }));
}

unsigned AudioEngine::sample_rate() const noexcept {
    return impl_->stream_sample_rate.load(std::memory_order_acquire);
}

unsigned AudioEngine::period_frames() const noexcept {
    return impl_->stream_period_frames.load(std::memory_order_acquire);
}

bool AudioEngine::healthy() const noexcept {
    return impl_->stream_healthy.load(std::memory_order_acquire);
}

}  // namespace piano
