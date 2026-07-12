#pragma once

#include <array>
#include <span>
#include <string_view>

namespace piano {

struct NoteBinding {
    int virtual_key;
    int semitone;
    std::string_view key_label;
    std::string_view note_name;
};

[[nodiscard]] std::span<const NoteBinding> note_bindings() noexcept;

class KeyboardState {
public:
    void sample() noexcept;

    [[nodiscard]] bool down(int virtual_key) const noexcept;
    [[nodiscard]] bool pressed(int virtual_key) const noexcept;
    [[nodiscard]] bool released(int virtual_key) const noexcept;

private:
    std::array<bool, 256> current_{};
    std::array<bool, 256> previous_{};
};

}  // namespace piano
