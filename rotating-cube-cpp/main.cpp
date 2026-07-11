#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

namespace {
constexpr int width = 80;
constexpr int height = 40;
constexpr float cube_size = 1.0f;
constexpr float surface_step = 0.16f;
constexpr float camera_distance = 4.0f;
constexpr float scale = 28.0f;
constexpr auto frame_delay = 30ms;

std::atomic_bool running{true};

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

struct Angles {
    float x{};
    float y{};
    float z{};
};

class Terminal {
public:
    Terminal()
    {
        std::cout << "\x1b[2J\x1b[?25l";
    }

    ~Terminal()
    {
        std::cout << "\x1b[?25h\x1b[0m\n";
        std::cout.flush();
    }

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
};

class CubeRenderer {
public:
    void render(const Angles& angles)
    {
        screen_.fill(' ');
        zbuffer_.fill(0.0f);

        for (int face = 0; face < 6; ++face) {
            draw_face(angles, face);
        }

        std::cout << "\x1b[H";
        for (int y = 0; y < height; ++y) {
            std::cout.write(&screen_[y * width], width);
            std::cout.put('\n');
        }
        std::cout.flush();
    }

private:
    std::array<float, width * height> zbuffer_{};
    std::array<char, width * height> screen_{};

    [[nodiscard]] static Vec3 rotate(Vec3 point, const Angles& angles)
    {
        const float sin_x = std::sin(angles.x);
        const float cos_x = std::cos(angles.x);
        const float sin_y = std::sin(angles.y);
        const float cos_y = std::cos(angles.y);
        const float sin_z = std::sin(angles.z);
        const float cos_z = std::cos(angles.z);

        const Vec3 around_x{
            point.x,
            point.y * cos_x - point.z * sin_x,
            point.y * sin_x + point.z * cos_x,
        };

        const Vec3 around_y{
            around_x.x * cos_y + around_x.z * sin_y,
            around_x.y,
            -around_x.x * sin_y + around_x.z * cos_y,
        };

        return {
            around_y.x * cos_z - around_y.y * sin_z,
            around_y.x * sin_z + around_y.y * cos_z,
            around_y.z,
        };
    }

    void plot(Vec3 point, char shade)
    {
        const float depth = point.z + camera_distance;
        if (depth <= 0.1f) {
            return;
        }

        const int sx = static_cast<int>(width / 2.0f + (point.x / depth) * scale * 2.0f);
        const int sy = static_cast<int>(height / 2.0f - (point.y / depth) * scale);

        if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
            return;
        }

        const int index = sy * width + sx;
        const float inv_depth = 1.0f / depth;
        if (inv_depth > zbuffer_[index]) {
            zbuffer_[index] = inv_depth;
            screen_[index] = shade;
        }
    }

    void draw_face(const Angles& angles, int face)
    {
        for (float u = -cube_size; u <= cube_size; u += surface_step) {
            for (float v = -cube_size; v <= cube_size; v += surface_step) {
                const auto [point, shade] = face_point(face, u, v);
                plot(rotate(point, angles), shade);
            }
        }
    }

    [[nodiscard]] static std::pair<Vec3, char> face_point(int face, float u, float v)
    {
        switch (face) {
        case 0:
            return {{cube_size, u, v}, '@'};
        case 1:
            return {{-cube_size, u, v}, '%'};
        case 2:
            return {{u, cube_size, v}, '#'};
        case 3:
            return {{u, -cube_size, v}, '*'};
        case 4:
            return {{u, v, cube_size}, '+'};
        default:
            return {{u, v, -cube_size}, '.'};
        }
    }
};

void stop(int)
{
    running = false;
}
}

int main()
{
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    const Terminal terminal;
    CubeRenderer renderer;
    Angles angles;

    while (running) {
        renderer.render(angles);
        angles.x += 0.045f;
        angles.y += 0.030f;
        angles.z += 0.020f;
        std::this_thread::sleep_for(frame_delay);
    }
}
