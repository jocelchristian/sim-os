#pragma once

#include <memory>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <stb_image.h>

#include "gui/Gui.hpp"
#include "simulations/Scheduler.hpp"

template<typename SchedulePolicy>
class [[nodiscard]] SchedulerApp final
{
  public:
    constexpr static auto WINDOW_WIDTH     = 1920;
    constexpr static auto WINDOW_HEIGHT    = 1080;
    constexpr static auto BACKGROUND_COLOR = ImVec4(0.94, 0.94, 0.94, 1.0);
    constexpr static auto BUTTON_SIZE      = ImVec2(16, 16);

  public:
    [[nodiscard]] static auto create(const auto& sim) -> std::unique_ptr<SchedulerApp>
    {
        const auto window = Gui::init_window("sim-os: scheduler", WINDOW_WIDTH, WINDOW_HEIGHT);
        if (!window) { return nullptr; }
        return std::unique_ptr<SchedulerApp>(new SchedulerApp { *window, sim });
    }

    void render()
    {
        while (!quit) {
            stepped_this_frame = false;
            if (glfwWindowShouldClose(window) == 1) { quit = true; }

            if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                if (!sim->complete() && !stepped_this_frame) {
                    sim->step();
                    stepped_this_frame = true;
                }
            }

            glfwPollEvents();
            if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) { continue; }

            Gui::new_frame();

            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            Gui::window(
              "sim-os: scheduler",
              Gui::WindowFlags::NoDecoration | Gui::WindowFlags::NoResize | Gui::WindowFlags::NoMove,
              [this] -> void {
                  draw_control_buttons();

                  const auto spacing                = ImGui::GetStyle().ItemSpacing.x;
                  const auto available_screen_space = ImGui::GetContentRegionAvail();
                  const auto group_width            = (available_screen_space.x - spacing * 2) / 3.0F;
                  const auto group_height           = available_screen_space.y - 30.0F;

                  Gui::group([&] -> void {
                      draw_process_queue("Ready", sim->ready, ImVec2(group_width, group_height));
                  });

                  ImGui::SameLine();

                  Gui::group([&] -> void {
                      draw_process_queue("Waiting", sim->waiting, ImVec2(group_width, (group_height / 2.0F) - 16.0F));
                      draw_process_queue("Arrival", sim->processes, ImVec2(group_width, (group_height / 2.0F) - 16.0F));
                  });

                  ImGui::SameLine();

                  Gui::group([&] -> void {
                      draw_running_process(ImVec2(group_width, (group_height / 2.0F) - 16.0F));
                      draw_statistics(ImVec2(group_width, (group_height / 2.0F) - 16.0F));
                  });
              }
            );

            Gui::draw_call(window, BACKGROUND_COLOR);
        }
    }

    void draw_statistics(const ImVec2& child_size)
    {
        const auto draw_table_element = [](const auto& key, const auto& value) -> void {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            Gui::text("{}", key);

            ImGui::TableSetColumnIndex(1);
            Gui::text("{}", value);
        };

        Gui::title("Stats", child_size, [&] -> void {
            Gui::child(
              "Simulation Statistics",
              child_size,
              Gui::ChildFlags::Border,
              Gui::WindowFlags::AlwaysVerticalScrollbar,
              [&] -> void {
                  if (ImGui::BeginTable("Stats Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

                      draw_table_element("Scheduler policy", SchedulePolicy::POLICY_NAME);
                      draw_table_element("Ready queue size", sim->ready.size());
                      draw_table_element("Waiting queue size", sim->waiting.size());
                      draw_table_element("Arrival size", sim->processes.size());
                      draw_table_element("Timer", sim->timer);

                      ImGui::EndTable();
                  }
              }
            );
        });
    }

    void draw_control_buttons()
    {
        constexpr static auto buttons_count = 2;
        Gui::center_content_horizontally(BUTTON_SIZE.x * buttons_count);

        // If icons could not be loaded fallback
        if (!maybe_previous_texture_id || !maybe_next_texture_id) {
            // TODO: implement previous
            Gui::button("Previous", [] -> void {});

            ImGui::SameLine();

            Gui::button("Next", [this] -> void {
                if (!sim->complete()) { sim->step(); }
            });
        } else {
            // TODO: implement previous
            auto* const previous_texture_id =
              reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_previous_texture_id));
            Gui::image_button(previous_texture_id, BUTTON_SIZE, [] -> void {});

            ImGui::SameLine();

            auto* const next_texture_id = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_next_texture_id));
            Gui::image_button(next_texture_id, BUTTON_SIZE, [this] -> void {
                if (!sim->complete()) { sim->step(); }
            });
        }
    }

    static void draw_process(const auto& process)
    {
        if (process == nullptr) { return; }

        Gui::child(
          "###Scrollable Process",
          Gui::ChildFlags::Border,
          Gui::WindowFlags::AlwaysVerticalScrollbar,
          [&] -> void {
              if (ImGui::CollapsingHeader(std::string { process->name }.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::Indent();

                  Gui::text("Pid: {}", process->pid);
                  Gui::text("Arrival Time: {}", process->arrival);

                  draw_events_table(process->events);

                  ImGui::Unindent();
              }
          }
        );
    }

    void draw_running_process(const ImVec2& child_size)
    {
        Gui::title("Running", child_size, [&] -> void {
            Gui::child(
              "###Scrollable Process",
              child_size,
              Gui::ChildFlags::Border,
              Gui::WindowFlags::AlwaysVerticalScrollbar,
              [&] -> void {
                  if (sim->running != nullptr) {
                      if (ImGui::CollapsingHeader(
                            std::string { sim->running->name }.c_str(), ImGuiTreeNodeFlags_DefaultOpen
                          )) {
                          ImGui::Indent();

                          Gui::text("Pid: {}", sim->running->pid);
                          Gui::text("Arrival Time: {}", sim->running->arrival);

                          draw_events_table(sim->running->events);

                          ImGui::Unindent();
                      }
                  }
              }
            );
        });
    }

    static void draw_process_queue(const std::string& title, const auto& processes, const ImVec2& child_size)
    {
        Gui::title(title, child_size, [&] -> void {
            Gui::child(
              title,
              child_size,
              Gui::ChildFlags::Border,
              Gui::WindowFlags::AlwaysVerticalScrollbar,
              [&] -> void {
                  for (const auto& process : processes) { draw_process(process); }
              }
            );
        });
    }

    static void draw_events_table(const Os::Process::EventsQueue& events)
    {
        if (!events.empty()) {
            if (ImGui::BeginTable("EventsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Event");
                ImGui::TableSetupColumn("Duration");
                ImGui::TableHeadersRow();

                for (const auto& event : events) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    Gui::text("{}", event.kind);

                    ImGui::TableSetColumnIndex(1);
                    Gui::text("{}", event.duration);
                }

                ImGui::EndTable();
            }
        }
    }

    ~SchedulerApp()
    {
        Gui::shutdown(window);
    }

    SchedulerApp(const SchedulerApp&)            = delete;
    SchedulerApp& operator=(const SchedulerApp&) = delete;
    SchedulerApp(SchedulerApp&&)                 = delete;
    SchedulerApp& operator=(SchedulerApp&&)      = delete;

  private:
    explicit SchedulerApp(GLFWwindow* window, const std::shared_ptr<Simulations::Scheduler<SchedulePolicy>>& sim)
      : window { window },
        sim { sim }
    {
        maybe_previous_texture_id = Gui::load_texture("resources/previous.png");
        maybe_next_texture_id     = Gui::load_texture("resources/next.png");
    }

  private:
    GLFWwindow*                                             window = nullptr;
    bool                                                    quit   = false;
    std::shared_ptr<Simulations::Scheduler<SchedulePolicy>> sim;
    bool                                                    stepped_this_frame = false;
    std::optional<GLuint>                                   maybe_previous_texture_id;
    std::optional<GLuint>                                   maybe_next_texture_id;
};
