#pragma once

#include <memory>

#include <imgui.h>

#include "gui/Gui.hpp"
#include "simulations/Scheduler.hpp"

class [[nodiscard]] Application final
{
  public:
    [[nodiscard]] static auto create(const std::shared_ptr<Simulations::Scheduler>& sim)
      -> std::unique_ptr<Application>;

    void render();

    void draw_save_button() const;
    void draw_control_buttons();
    void draw_scheduler_policy_picker();

    void draw_running_process(const ImVec2& child_size) const;
    void draw_graphs(const ImVec2& child_size);
    void draw_cpu_usage_graph(const ImVec2& child_size);
    void draw_throughput_graph(const ImVec2& child_size);
    void draw_average_waiting_time_graph(const ImVec2& child_size);
    void draw_average_turnaround_time_graph(const ImVec2& child_size);
    void draw_statistics(const ImVec2& child_size) const;

    ~Application();
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

  private:
    explicit Application(GLFWwindow* window, const std::shared_ptr<Simulations::Scheduler>& sim);

  private:
    constexpr static auto WINDOW_WIDTH     = 1920;
    constexpr static auto WINDOW_HEIGHT    = 1080;
    constexpr static auto BACKGROUND_COLOR = Gui::hex_colour_to_imvec4(0x181818);
    constexpr static auto BUTTON_SIZE      = ImVec2(16, 16);
    constexpr static auto PLOT_HISTORY     = 10.0F;

  private:
    GLFWwindow*                             window = nullptr;
    bool                                    quit   = false;
    std::shared_ptr<Simulations::Scheduler> sim;
    bool                                    should_finish      = false;
    bool                                    stepped_this_frame = false;
    Gui::Texture                            restart_texture;
    Gui::Texture                            play_texture;
    Gui::Texture                            next_texture;
    Gui::Texture                            save_texture;
    float                                   delta_time = 0.0F;
    Gui::Plotting::RingBuffer               cpu_usage_buffer;
    Gui::Plotting::RingBuffer               average_waiting_time_buffer;
    std::size_t                             max_waiting_time = 0;
    Gui::Plotting::RingBuffer               average_turnaround_time_buffer;
    std::size_t                             max_turnaround_time = 0;
    Gui::Plotting::RingBuffer               throughput_buffer;
    double                                  max_throughput = 0;
};
