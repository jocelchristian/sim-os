#pragma once

#include <memory>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <implot.h>

#include <stb_image.h>

#include "gui/Gui.hpp"
#include "simulations/Scheduler.hpp"

class [[nodiscard]] ScrollingBuffer final
{
  public:
    explicit ScrollingBuffer(int capacity = 2000)
      : capacity { capacity }
    {
        data.reserve(capacity);
    }

    void emplace_point(const float x, const float y)
    {
        if (data.size() < capacity) {
            data.push_back(ImVec2(x, y));
            return;
        }

        data[cursor] = ImVec2(x, y);
        cursor       = (cursor + 1) % capacity;
    }

    [[nodiscard]] auto operator[](const int index) const -> const ImVec2& { return data[index]; }

    [[nodiscard]] auto size() const -> int { return data.size(); }

    [[nodiscard]] auto offset() const -> int { return cursor; }

  private:
    int              capacity;
    int              cursor = 0;
    ImVector<ImVec2> data;
};

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
        ImPlot::CreateContext();

        return std::unique_ptr<SchedulerApp>(new SchedulerApp { *window, sim });
    }

    void render()
    {
        while (!quit) {
            stepped_this_frame = false;
            if (glfwWindowShouldClose(window) == 1) { quit = true; }

            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) { should_finish = true; }

            if (!sim->complete() && should_finish && !stepped_this_frame) {
                sim->step();
                stepped_this_frame = true;
            }

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
              [this] {
                  draw_control_buttons();

                  const auto spacing                = ImGui::GetStyle().ItemSpacing.x;
                  const auto available_screen_space = ImGui::GetContentRegionAvail();
                  const auto group_width            = (available_screen_space.x - spacing * 2) / 3.0F;
                  const auto group_height           = available_screen_space.y - 30.0F;
                  const auto child_size             = ImVec2(group_width, (group_height / 2.0F) - 16.0F);

                  Gui::group([&] {
                      draw_process_queue("Ready", sim->ready, child_size);
                      draw_process_queue("Arrival", sim->processes, child_size);
                  });

                  ImGui::SameLine();

                  Gui::group([&] {
                      draw_process_queue("Waiting", sim->waiting, child_size);
                      draw_graphs(child_size);
                  });

                  ImGui::SameLine();

                  Gui::group([&] {
                      draw_running_process(child_size);
                      draw_statistics(child_size);
                  });
              }
            );

            Gui::draw_call(window, BACKGROUND_COLOR);
        }
    }

    void draw_statistics(const ImVec2& child_size)
    {
        constexpr static auto TABLE_NAME = "StatsTable";
        constexpr static auto HEADERS    = { "Key", "Value" };

        Gui::title("Stats", child_size, [&] {
            Gui::child(
              "Simulation Statistics",
              child_size,
              Gui::ChildFlags::Border,
              Gui::WindowFlags::AlwaysVerticalScrollbar,
              [&] {
                  Gui::draw_table(TABLE_NAME, HEADERS, Gui::TableFlags::Borders | Gui::TableFlags::RowBackground, [&] {
                      const auto draw_key_value = [](const std::string_view key, const auto& value) {
                          Gui::draw_table_row([&] { Gui::text("{}", key); }, [&] { Gui::text("{}", value); });
                      };

                      draw_key_value("Scheduler Policy", SchedulePolicy::POLICY_NAME);
                      draw_key_value("Ready queue size", sim->ready.size());
                      draw_key_value("Waiting queue size", sim->waiting.size());
                      draw_key_value("Arrival size", sim->processes.size());
                      draw_key_value("Timer", sim->timer);
                  });
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
            Gui::button("Previous", [] {});

            ImGui::SameLine();

            Gui::button("Next", [this] {
                if (!sim->complete()) { should_finish = true; }
            });

            ImGui::SameLine();

            Gui::button("Next", [this] {
                if (!sim->complete()) { sim->step(); }
            });
        } else {
            // TODO: implement previous
            auto* const previous_texture_id =
              reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_previous_texture_id));
            Gui::image_button(previous_texture_id, BUTTON_SIZE, [] {});

            ImGui::SameLine();

            auto* const play_texture_id = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_play_texture_id));
            Gui::image_button(play_texture_id, BUTTON_SIZE, [this] {
                if (!sim->complete()) { should_finish = true; }
            });

            ImGui::SameLine();

            auto* const next_texture_id = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_next_texture_id));
            Gui::image_button(next_texture_id, BUTTON_SIZE, [this] {
                if (!sim->complete()) { sim->step(); }
            });
        }
    }

    static void draw_process(const auto& process)
    {
        if (process == nullptr) { return; }

        Gui::child("###Scrollable Process", Gui::ChildFlags::Border, Gui::WindowFlags::AlwaysVerticalScrollbar, [&] {
            auto header_title = std::format("{} #{}", process->name, process->pid);
            if (process->name != "Process") { header_title = std::string { process->name }; }

            Gui::collapsing(header_title, Gui::TreeNodeFlags::DefaultOpen, [&] {
                Gui::text("Pid: {}", process->pid);
                Gui::text("Arrival Time: {}", process->arrival);
                draw_events_table(process->events);
            });
        });
    }

    void draw_running_process(const ImVec2& child_size)
    {
        Gui::title("Running", child_size, [&] {
            Gui::child(
              "###Scrollable Process",
              child_size,
              Gui::ChildFlags::Border,
              Gui::WindowFlags::AlwaysVerticalScrollbar,
              [&] {
                  if (sim->running != nullptr) {
                      const auto name = std::string { sim->running->name };
                      Gui::collapsing(name, Gui::TreeNodeFlags::DefaultOpen, [&] {
                          Gui::text("Pid: {}", sim->running->pid);
                          Gui::text("Arrival Time: {}", sim->running->arrival);
                          draw_events_table(sim->running->events);
                      });
                  }
              }
            );
        });
    }

    static void draw_process_queue(const std::string& title, const auto& processes, const ImVec2& child_size)
    {
        Gui::title(title, child_size, [&] {
            Gui::child(title, child_size, Gui::ChildFlags::Border, Gui::WindowFlags::AlwaysVerticalScrollbar, [&] {
                for (const auto& process : processes) { draw_process(process); }
            });
        });
    }

    static void draw_events_table(const Os::Process::EventsQueue& events)
    {
        constexpr static auto TABLE_NAME = "EventsTable";
        constexpr static auto HEADERS    = { "Event", "Duration", "Resource Usage" };

        if (!events.empty()) {
            Gui::draw_table(TABLE_NAME, HEADERS, Gui::TableFlags::Borders | Gui::TableFlags::RowBackground, [&] {
                for (const auto& event : events) {
                    Gui::draw_table_row(
                      [&] { Gui::text("{}", event.kind); },
                      [&] { Gui::text("{}", event.duration); },
                      [&] { Gui::text("{}%", std::lround(event.resource_usage * 100)); }
                    );
                }
            });
        }
    }

    void draw_graphs(const ImVec2& child_size)
    {
        constexpr static auto HISTORY = 10.0F;

        static float           t = 0.0F;
        static ScrollingBuffer cpu_usage;

        if (!sim->complete()) {
            t += ImGui::GetIO().DeltaTime;
            cpu_usage.emplace_point(t, static_cast<std::size_t>(sim->cpu_usage * 100));
        }

        Gui::title("Graphs", child_size, [&] {
            if (ImPlot::BeginPlot("##Scrolling", child_size)) {
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks, 0);
                ImPlot::SetupAxisLimits(ImAxis_X1, t - HISTORY, t, ImGuiCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100);
                ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(1));
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.5F);

                ImPlot::PlotLine(
                  "cpu usage %",
                  &cpu_usage[0].x,
                  &cpu_usage[0].y,
                  cpu_usage.size(),
                  0,
                  cpu_usage.offset(),
                  2 * sizeof(float)
                );

                ImPlot::PopStyleVar();
                ImPlot::PopStyleColor();
                ImPlot::EndPlot();
            }
        });
    }

    ~SchedulerApp()
    {
        Gui::shutdown(window);
        ImPlot::DestroyContext();
        glDeleteTextures(1, &(*maybe_previous_texture_id));
        glDeleteTextures(1, &(*maybe_play_texture_id));
        glDeleteTextures(1, &(*maybe_next_texture_id));
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
        maybe_play_texture_id     = Gui::load_texture("resources/play.png");
        maybe_next_texture_id     = Gui::load_texture("resources/next.png");
    }

  private:
    GLFWwindow*                                             window = nullptr;
    bool                                                    quit   = false;
    std::shared_ptr<Simulations::Scheduler<SchedulePolicy>> sim;
    bool                                                    should_finish      = false;
    bool                                                    stepped_this_frame = false;
    std::optional<GLuint>                                   maybe_previous_texture_id;
    std::optional<GLuint>                                   maybe_play_texture_id;
    std::optional<GLuint>                                   maybe_next_texture_id;
};
