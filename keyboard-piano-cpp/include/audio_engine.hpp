#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace piano {

enum class Timbre {
    grand_piano,
    electric_piano,
    tonewheel_organ,
    warm_pad,
};

enum class DrumSound {
    kick,
    snare,
    closed_hat,
    open_hat,
    low_tom,
    high_tom,
    clap,
    crash,
};

[[nodiscard]] std::string_view timbre_name(Timbre timbre) noexcept;
[[nodiscard]] std::string_view drum_name(DrumSound drum) noexcept;

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    [[nodiscard]] bool start(std::string& error);
    void stop() noexcept;

    void note_on(int midi_note, float velocity = 0.85F);
    void note_off(int midi_note);
    void trigger_drum(DrumSound drum, float velocity = 0.92F);
    void all_notes_off();

    void set_sustain(bool enabled);
    [[nodiscard]] bool sustain() const;

    void set_timbre(Timbre timbre) noexcept;
    [[nodiscard]] Timbre timbre() const noexcept;

    void set_master_volume(float volume) noexcept;
    [[nodiscard]] float master_volume() const noexcept;

    [[nodiscard]] int active_voice_count() const;
    [[nodiscard]] unsigned sample_rate() const noexcept;
    [[nodiscard]] unsigned period_frames() const noexcept;
    [[nodiscard]] bool healthy() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace piano
