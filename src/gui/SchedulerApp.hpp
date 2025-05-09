#pragma once

#include <memory>

#include <imgui.h>
#include "GLFW/glfw3.h"
#include "simulations/Scheduler.hpp"

namespace Gui
{

class [[nodiscard]] SchedulerApp final
{
  public:
    constexpr static auto WINDOW_WIDTH  = 1920;
    constexpr static auto WINDOW_HEIGHT = 1080;
    constexpr static auto GLSL_VERSION  = "#version 330";

  public:
    [[nodiscard]] static auto create(const std::shared_ptr<Simulations::Scheduler>& sim)
      -> std::unique_ptr<SchedulerApp>;

    void render();

    void draw_statistics(const ImVec2& child_size);
    void draw_control_buttons();

    ~SchedulerApp();

    SchedulerApp(const SchedulerApp&)            = delete;
    SchedulerApp& operator=(const SchedulerApp&) = delete;
    SchedulerApp(SchedulerApp&&)                 = delete;
    SchedulerApp& operator=(SchedulerApp&&)      = delete;

  private:
    explicit SchedulerApp(GLFWwindow* window, const std::shared_ptr<Simulations::Scheduler>& sim);

  private:
    GLFWwindow*                             window = nullptr;
    bool                                    quit   = false;
    std::shared_ptr<Simulations::Scheduler> sim;
    bool                                    stepped_this_frame = false;
    std::optional<GLuint> maybe_previous_texture_id;
    std::optional<GLuint> maybe_next_texture_id;
};

} // namespace Gui
