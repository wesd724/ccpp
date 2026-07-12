#include "keyboard.hpp"

#include <windows.h>

#include <array>
#include <cstddef>

namespace piano {
namespace {

constexpr std::array<NoteBinding, 17> bindings{{
    {'A', 0, "A", "C"},
    {'W', 1, "W", "C#"},
    {'S', 2, "S", "D"},
    {'E', 3, "E", "D#"},
    {'D', 4, "D", "E"},
    {'F', 5, "F", "F"},
    {'T', 6, "T", "F#"},
    {'G', 7, "G", "G"},
    {'Y', 8, "Y", "G#"},
    {'H', 9, "H", "A"},
    {'U', 10, "U", "A#"},
    {'J', 11, "J", "B"},
    {'K', 12, "K", "C"},
    {'O', 13, "O", "C#"},
    {'L', 14, "L", "D"},
    {'P', 15, "P", "D#"},
    {VK_OEM_1, 16, ";", "E"},
}};

constexpr std::array<int, 29> tracked_keys{{
    'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 'K', 'O', 'L', 'P',
    VK_OEM_1, VK_ESCAPE, VK_TAB, 'Z', 'X', VK_UP, VK_DOWN, '1', '2', '3', '4', VK_SPACE,
    'R',
}};

[[nodiscard]] bool valid_key(int virtual_key) noexcept {
    return virtual_key >= 0 && virtual_key < 256;
}

}  // namespace

std::span<const NoteBinding> note_bindings() noexcept {
    return bindings;
}

void KeyboardState::sample() noexcept {
    previous_ = current_;
    for (const int virtual_key : tracked_keys) {
        current_[static_cast<std::size_t>(virtual_key)] =
            (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
    }
}

bool KeyboardState::down(int virtual_key) const noexcept {
    return valid_key(virtual_key) && current_[static_cast<std::size_t>(virtual_key)];
}

bool KeyboardState::pressed(int virtual_key) const noexcept {
    if (!valid_key(virtual_key)) {
        return false;
    }
    const auto key = static_cast<std::size_t>(virtual_key);
    return current_[key] && !previous_[key];
}

bool KeyboardState::released(int virtual_key) const noexcept {
    if (!valid_key(virtual_key)) {
        return false;
    }
    const auto key = static_cast<std::size_t>(virtual_key);
    return !current_[key] && previous_[key];
}

}  // namespace piano
