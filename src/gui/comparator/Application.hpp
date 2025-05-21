#pragma once

#include <memory>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <gui/Gui.hpp>

class [[nodiscard]] Application final
{
  public:
    [[nodiscard]] static auto create(
      const std::span<const std::string>                             labels,
      const std::span<const std::unordered_map<std::string, std::string>>& values
    ) -> std::unique_ptr<Application>;

    void render();

    void draw_bar_charts() const;

    ~Application();
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

  private:
    Application(
      GLFWwindow*                                                 window,
      const std::span<const std::string>                          labels,
      const std::unordered_map<std::string, std::vector<double>>& tables
    );

  private:
    constexpr static auto WINDOW_WIDTH     = 1920;
    constexpr static auto WINDOW_HEIGHT    = 1080;
    constexpr static auto BACKGROUND_COLOR = Gui::hex_colour_to_imvec4(0x181818);
    constexpr static auto BUTTON_SIZE      = ImVec2(16, 16);

  private:
    GLFWwindow* window = nullptr;
    bool        quit   = false;

    std::span<const std::string>                         labels;
    std::unordered_map<std::string, std::vector<double>> tables;
};
