#include "audio_engine.hpp"
#include "keyboard.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace {

using namespace std::chrono_literals;

std::atomic_bool keep_running{true};

enum class InstrumentMode {
    piano,
    drum_kit,
};

struct DrumBinding {
    int virtual_key;
    std::string_view key_label;
    piano::DrumSound sound;
};

constexpr std::array<DrumBinding, 8> drum_bindings{{
    {'A', "A", piano::DrumSound::kick},
    {'S', "S", piano::DrumSound::snare},
    {'D', "D", piano::DrumSound::closed_hat},
    {'F', "F", piano::DrumSound::open_hat},
    {'G', "G", piano::DrumSound::low_tom},
    {'H', "H", piano::DrumSound::high_tom},
    {'J', "J", piano::DrumSound::clap},
    {'K', "K", piano::DrumSound::crash},
}};

BOOL WINAPI console_control_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) {
        keep_running.store(false, std::memory_order_release);
        return TRUE;
    }
    return FALSE;
}

class ConsoleSession {
public:
    ConsoleSession()
        : input_(GetStdHandle(STD_INPUT_HANDLE)), output_(GetStdHandle(STD_OUTPUT_HANDLE)) {
        SetConsoleTitleW(L"Keyboard Studio - WASAPI Synthesizer");
        SetConsoleCtrlHandler(console_control_handler, TRUE);

        if (input_ != INVALID_HANDLE_VALUE && GetConsoleMode(input_, &original_input_mode_)) {
            input_mode_saved_ = true;
            DWORD mode = original_input_mode_;
            mode |= ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT;
            mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_QUICK_EDIT_MODE);
            SetConsoleMode(input_, mode);
            FlushConsoleInputBuffer(input_);
        }
        if (output_ != INVALID_HANDLE_VALUE && GetConsoleMode(output_, &original_output_mode_)) {
            output_mode_saved_ = true;
            SetConsoleMode(output_, original_output_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        if (output_ != INVALID_HANDLE_VALUE && GetConsoleCursorInfo(output_, &original_cursor_)) {
            cursor_saved_ = true;
            CONSOLE_CURSOR_INFO cursor = original_cursor_;
            cursor.bVisible = FALSE;
            SetConsoleCursorInfo(output_, &cursor);
        }
        std::cout << "\x1b[2J\x1b[H\x1b[?25l" << std::flush;
    }

    ~ConsoleSession() {
        std::cout << "\x1b[0m\x1b[2J\x1b[H\x1b[?25h" << std::flush;
        if (cursor_saved_) {
            SetConsoleCursorInfo(output_, &original_cursor_);
        }
        if (output_mode_saved_) {
            SetConsoleMode(output_, original_output_mode_);
        }
        if (input_mode_saved_) {
            SetConsoleMode(input_, original_input_mode_);
            FlushConsoleInputBuffer(input_);
        }
        SetConsoleCtrlHandler(console_control_handler, FALSE);
    }

    ConsoleSession(const ConsoleSession&) = delete;
    ConsoleSession& operator=(const ConsoleSession&) = delete;

    void discard_buffered_input() const noexcept {
        if (input_mode_saved_) {
            FlushConsoleInputBuffer(input_);
        }
    }

private:
    HANDLE input_ = INVALID_HANDLE_VALUE;
    HANDLE output_ = INVALID_HANDLE_VALUE;
    DWORD original_input_mode_ = 0;
    DWORD original_output_mode_ = 0;
    CONSOLE_CURSOR_INFO original_cursor_{};
    bool input_mode_saved_ = false;
    bool output_mode_saved_ = false;
    bool cursor_saved_ = false;
};

[[nodiscard]] int midi_note_for(int octave, int semitone) noexcept {
    return 12 * (octave + 1) + semitone;
}

[[nodiscard]] std::string midi_note_name(int midi_note) {
    constexpr std::array<std::string_view, 12> names{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int pitch_class = ((midi_note % 12) + 12) % 12;
    const int octave = midi_note / 12 - 1;
    return std::string(names[static_cast<std::size_t>(pitch_class)]) + std::to_string(octave);
}

void draw_screen(const piano::AudioEngine& audio, const piano::KeyboardState& keyboard,
                 InstrumentMode mode, int octave, const std::array<int, 17>& active_notes) {
    std::ostringstream screen;
    const unsigned rate = audio.sample_rate();
    const unsigned period = audio.period_frames();
    const double latency_ms = rate == 0 ? 0.0 :
        1000.0 * static_cast<double>(period) / static_cast<double>(rate);

    screen << "\x1b[H"
           << "+------------------------------------------------------------------------------+\n"
           << "|                         KEYBOARD STUDIO                                     |\n"
           << "+------------------------------------------------------------------------------+\n";
    if (mode == InstrumentMode::piano) {
        screen << "| PIANO     [W] [E]     [T] [Y] [U]     [O] [P]                              |\n"
               << "|        [A] [S] [D] [F] [G] [H] [J] [K] [L] [;]                             |\n"
               << "|         C   D   E   F   G   A   B   C   D   E                              |\n";
    } else {
        screen << "| DRUM KIT   [A] Kick     [S] Snare      [D] Closed Hat   [F] Open Hat       |\n"
               << "|            [G] Low Tom  [H] High Tom   [J] Clap         [K] Crash          |\n"
               << "|            One-shot procedural drums; pads may overlap                     |\n";
    }
    screen << "+------------------------------------------------------------------------------+\n";

    if (mode == InstrumentMode::piano) {
        screen << "  Mode    : PIANO      Timbre: " << std::left << std::setw(18)
               << piano::timbre_name(audio.timbre()) << " Octave: " << octave
               << "  Sustain: " << (audio.sustain() ? "ON " : "OFF") << '\n';
    } else {
        screen << "  Mode    : DRUM KIT   Synthesis: Kick / Snare / Hats / Toms / Clap / Crash\n";
    }
    screen << "  Volume  : " << std::right << std::setw(3)
           << static_cast<int>(std::lround(audio.master_volume() * 100.0F)) << "%"
           << "       Voices: " << audio.active_voice_count() << "/48\n";
    screen << "  Audio  : WASAPI IAudioClient3 | " << rate << " Hz | " << period
           << " frames | " << std::fixed << std::setprecision(2) << latency_ms << " ms period\n";
    screen << "  Pressed : ";

    bool any_pressed = false;
    if (mode == InstrumentMode::piano) {
        const auto bindings = piano::note_bindings();
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            if (active_notes[i] >= 0 && keyboard.down(bindings[i].virtual_key)) {
                if (any_pressed) {
                    screen << ' ';
                }
                screen << bindings[i].key_label << ':' << midi_note_name(active_notes[i]);
                any_pressed = true;
            }
        }
    } else {
        for (const DrumBinding& binding : drum_bindings) {
            if (keyboard.down(binding.virtual_key)) {
                if (any_pressed) {
                    screen << ' ';
                }
                screen << binding.key_label << ':' << piano::drum_name(binding.sound);
                any_pressed = true;
            }
        }
    }
    if (!any_pressed) {
        screen << '-';
    }

    screen << "\n\n"
           << "  Tab         switch Piano / Drum Kit   Up / Down   volume\n"
           << "  R           silence every voice       Esc         quit\n";
    if (mode == InstrumentMode::piano) {
        screen << "  Z / X       octave down / up           Space       sustain pedal\n"
               << "  Timbres: [1] Grand  [2] Electric  [3] Organ  [4] Warm Pad\n"
               << "  Play several note keys together for chords.";
    } else {
        screen << "  A/S/D/F/G/H/J/K trigger the eight drum pads.\n"
               << "  Repeated hits and cymbal tails share the same 48-voice mixer.";
    }

    const std::string frame = screen.str();
    std::string output;
    output.reserve(frame.size() + 128);
    for (const char character : frame) {
        if (character == '\n') {
            output += "\x1b[K\n";
        } else {
            output += character;
        }
    }
    output += "\x1b[K\x1b[J";
    std::cout << output << std::flush;
}

void select_timbre(piano::AudioEngine& audio, const piano::KeyboardState& keyboard) {
    if (keyboard.pressed('1')) {
        audio.set_timbre(piano::Timbre::grand_piano);
    } else if (keyboard.pressed('2')) {
        audio.set_timbre(piano::Timbre::electric_piano);
    } else if (keyboard.pressed('3')) {
        audio.set_timbre(piano::Timbre::tonewheel_organ);
    } else if (keyboard.pressed('4')) {
        audio.set_timbre(piano::Timbre::warm_pad);
    }
}

int check_audio() {
    piano::AudioEngine audio;
    std::string error;
    if (!audio.start(error)) {
        std::cerr << "Audio check failed: " << error << '\n';
        return 1;
    }
    const double period_ms = 1000.0 * static_cast<double>(audio.period_frames()) /
                             static_cast<double>(audio.sample_rate());
    std::cout << "WASAPI IAudioClient3 ready: " << audio.sample_rate() << " Hz, "
              << audio.period_frames() << " frames, " << std::fixed << std::setprecision(2)
              << period_ms << " ms period\n";
    audio.stop();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--check-audio") {
        return check_audio();
    }
    if (argc == 2 && (std::string_view(argv[1]) == "--help" ||
                      std::string_view(argv[1]) == "-h")) {
        std::cout << "Usage: keyboard_piano [--check-audio]\n";
        return 0;
    }
    if (argc != 1) {
        std::cerr << "Unknown option. Use --help.\n";
        return 2;
    }

    piano::AudioEngine audio;
    std::string error;
    if (!audio.start(error)) {
        std::cerr << "Could not start audio: " << error << '\n';
        return 1;
    }

    ConsoleSession console;
    piano::KeyboardState keyboard;
    std::array<int, 17> active_notes;
    active_notes.fill(-1);
    InstrumentMode mode = InstrumentMode::piano;
    int octave = 4;
    auto next_draw = std::chrono::steady_clock::now();
    auto next_flush = next_draw;

    while (keep_running.load(std::memory_order_acquire) && audio.healthy()) {
        keyboard.sample();
        if (keyboard.pressed(VK_ESCAPE)) {
            break;
        }

        if (keyboard.pressed(VK_TAB)) {
            if (mode == InstrumentMode::piano) {
                for (const int midi_note : active_notes) {
                    if (midi_note >= 0) {
                        audio.note_off(midi_note);
                    }
                }
                active_notes.fill(-1);
                audio.set_sustain(false);
                mode = InstrumentMode::drum_kit;
            } else {
                mode = InstrumentMode::piano;
            }
        }
        if (mode == InstrumentMode::piano && keyboard.pressed('Z')) {
            octave = std::max(octave - 1, 1);
        }
        if (mode == InstrumentMode::piano && keyboard.pressed('X')) {
            octave = std::min(octave + 1, 7);
        }
        if (keyboard.pressed(VK_UP)) {
            audio.set_master_volume(audio.master_volume() + 0.05F);
        }
        if (keyboard.pressed(VK_DOWN)) {
            audio.set_master_volume(audio.master_volume() - 0.05F);
        }
        if (mode == InstrumentMode::piano) {
            select_timbre(audio, keyboard);
        }

        if (mode == InstrumentMode::piano && keyboard.pressed(VK_SPACE)) {
            audio.set_sustain(true);
        }
        if (mode == InstrumentMode::piano && keyboard.released(VK_SPACE)) {
            audio.set_sustain(false);
        }
        if (keyboard.pressed('R')) {
            audio.all_notes_off();
            active_notes.fill(-1);
        }

        if (mode == InstrumentMode::piano) {
            const auto bindings = piano::note_bindings();
            for (std::size_t i = 0; i < bindings.size(); ++i) {
                const piano::NoteBinding& binding = bindings[i];
                if (keyboard.pressed(binding.virtual_key)) {
                    const int midi_note = midi_note_for(octave, binding.semitone);
                    active_notes[i] = midi_note;
                    audio.note_on(midi_note);
                }
                if (keyboard.released(binding.virtual_key) && active_notes[i] >= 0) {
                    audio.note_off(active_notes[i]);
                    active_notes[i] = -1;
                }
            }
        } else {
            for (const DrumBinding& binding : drum_bindings) {
                if (keyboard.pressed(binding.virtual_key)) {
                    audio.trigger_drum(binding.sound);
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_draw) {
            draw_screen(audio, keyboard, mode, octave, active_notes);
            next_draw = now + 50ms;
        }
        if (now >= next_flush) {
            console.discard_buffered_input();
            next_flush = now + 250ms;
        }
        std::this_thread::sleep_for(2ms);
    }

    const bool audio_failed = !audio.healthy();
    audio.all_notes_off();
    audio.stop();
    if (!keep_running.load(std::memory_order_acquire)) {
        std::cout << "Interrupted.\n";
    } else if (audio_failed) {
        std::cout << "Audio stream stopped.\n";
    }
    return 0;
}
