#pragma once

#include <memory>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <implot.h>

#include <stb_image.h>

#include "gui/Gui.hpp"
#include "os/Os.hpp"
#include "simulations/Scheduler.hpp"
#include "Util.hpp"

template<typename SchedulePolicy>
class [[nodiscard]] SchedulerApp final
{
  public:
    constexpr static auto WINDOW_WIDTH     = 1920;
    constexpr static auto WINDOW_HEIGHT    = 1080;
    constexpr static auto BACKGROUND_COLOR = ImVec4(0.94, 0.94, 0.94, 1.0);
    constexpr static auto BUTTON_SIZE      = ImVec2(16, 16);
    constexpr static auto PLOT_HISTORY     = 10.0F;

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

            if (!sim->complete()) { delta_time += ImGui::GetIO().DeltaTime; }

            Gui::new_frame();

            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            Gui::window(
              "sim-os: scheduler",
              Gui::WindowFlags::NoDecoration | Gui::WindowFlags::NoResize | Gui::WindowFlags::NoMove,
              [this] {
                  draw_control_buttons();

                  const auto child_size = Gui::grid_layout_calc_size(2, 3);

                  Gui::group([&] {
                      auto ready = std::views::join(sim->ready) | std::ranges::to<std::deque>();
                      draw_process_queue("Ready", ready, child_size);

                      auto arrival = std::views::join(sim->processes) | std::ranges::to<std::deque>();
                      draw_process_queue("Arrival", arrival, child_size);
                  });

                  ImGui::SameLine();

                  Gui::group([&] {
                      auto waiting = std::views::join(sim->waiting) | std::ranges::to<std::deque>();
                      draw_process_queue("Waiting", waiting, child_size);
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

    void draw_statistics(const ImVec2& child_size) const
    {
        constexpr static auto CHILD_FLAGS  = Gui::ChildFlags::Border;
        constexpr static auto WINDOW_FLAGS = Gui::WindowFlags::AlwaysVerticalScrollbar;
        constexpr static auto TABLE_FLAGS  = Gui::TableFlags::Borders | Gui::TableFlags::RowBackground;

        Gui::title("Stats", child_size, [&](const auto& remaining_size) {
            Gui::child("Simulation Statistics", remaining_size, CHILD_FLAGS, WINDOW_FLAGS, [&] {
                constexpr static auto INFO_TABLE_HEADERS = { "Key", "Value" };
                Gui::draw_table("InfoTable", INFO_TABLE_HEADERS, TABLE_FLAGS, [&] {
                    const auto draw_key_value = [](const std::string_view key, const auto& value) {
                        Gui::draw_table_row([&] { Gui::text("{}", key); }, [&] { Gui::text("{}", value); });
                    };

                    draw_key_value("Timer", sim->timer);
                    draw_key_value("Scheduler Policy", SchedulePolicy::POLICY_NAME);
                });

                ImGui::Separator();

                constexpr static auto QUEUES_TABLE_HEADERS = { "Queue", "Size" };
                Gui::draw_table("QueuesTable", QUEUES_TABLE_HEADERS, TABLE_FLAGS, [&] {
                    const auto draw_key_value = [](const std::string_view key, const auto& value) {
                        Gui::draw_table_row([&] { Gui::text("{}", key); }, [&] { Gui::text("{}", value); });
                    };

                    const auto calculate_size = [](const auto& queues) -> std::size_t {
                        return std::accumulate(queues.begin(), queues.end(), 0, [](const auto& acc, const auto& queue) {
                            return acc + queue.size();
                        });
                    };

                    draw_key_value("Ready queue size", calculate_size(sim->ready));
                    draw_key_value("Waiting queue size", calculate_size(sim->waiting));
                    draw_key_value("Arrival size", calculate_size(sim->processes));
                });

                ImGui::Separator();

                constexpr static auto CPU_CORES_TABLE_HEADERS = { "CPU", "Usage" };
                Gui::draw_table("CpuCoresTable", CPU_CORES_TABLE_HEADERS, TABLE_FLAGS, [&] {
                    const auto draw_key_value = [](const std::string_view key, const auto& value) {
                        Gui::draw_table_row([&] { Gui::text("{}", key); }, [&] { Gui::text("{}%", value); });
                    };

                    for (std::size_t thread_idx = 0; thread_idx < sim->threads_count; ++thread_idx) {
                        draw_key_value(
                          std::format("Core #{}", thread_idx),
                          static_cast<std::size_t>(sim->cpu_usage[thread_idx] * 100)
                        );
                    }
                });

                ImGui::Separator();

                constexpr static auto METRICS_TABLE_HEADERS = { "Key", "Value" };
                Gui::draw_table("MetricsTable", METRICS_TABLE_HEADERS, TABLE_FLAGS, [&] {
                    const auto draw_key_value = [](const std::string_view key, const auto& value) {
                        Gui::draw_table_row([&] { Gui::text("{}", key); }, [&] { Gui::text("{}", value); });
                    };

                    draw_key_value("Avg. waiting time", sim->average_waiting_time());
                    draw_key_value("Max. waiting time", max_waiting_time);
                    draw_key_value("Avg. turnaround time", sim->average_turnaround_time());
                    draw_key_value("Max. turnaround time", max_turnaround_time);
                    draw_key_value("Avg. throughput", sim->throughput);
                    draw_key_value("Max. throughput", max_throughput);
                });
            });
        });
    }

    void draw_control_buttons()
    {
        constexpr static auto BUTTONS_COUNT = 2;
        Gui::center_content_horizontally(BUTTON_SIZE.x * BUTTONS_COUNT);

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

        constexpr static auto CHILD_FLAGS  = Gui::ChildFlags::Border;
        constexpr static auto WINDOW_FLAGS = Gui::WindowFlags::AlwaysVerticalScrollbar;

        Gui::child("###Scrollable Process", CHILD_FLAGS, WINDOW_FLAGS, [&] {
            auto header_title = std::format("{} #{}", process->name, process->pid);
            if (process->name != "Process") { header_title = std::string { process->name }; }

            Gui::collapsing(header_title, Gui::TreeNodeFlags::DefaultOpen, [&] {
                Gui::text("Pid: {}", process->pid);
                Gui::text("Arrival Time: {}", process->arrival);
                draw_events_table(process->events);
            });
        });
    }

    void draw_running_process(const ImVec2& child_size) const
    {
        constexpr static auto CHILD_FLAGS  = Gui::ChildFlags::Border;
        constexpr static auto WINDOW_FLAGS = Gui::WindowFlags::AlwaysVerticalScrollbar;

        const auto cols = static_cast<std::size_t>(std::ceil(std::sqrt(sim->threads_count)));
        const auto rows = static_cast<std::size_t>(std::ceil(static_cast<double>(sim->threads_count) / cols));
        const auto nested_child_size = Gui::grid_layout_calc_size(rows, cols, child_size);

        std::size_t idx = 0;
        for (std::size_t row = 0; row < rows; ++row) {
            for (std::size_t col = 0; col < cols; ++col) {
                const auto running = sim->running[idx];
                const auto title   = std::format("CPU Core #{}", idx);

                Gui::group([&] {
                    Gui::title(title, nested_child_size, [&](const auto& remaining_size) {
                        Gui::child(title, remaining_size, CHILD_FLAGS, WINDOW_FLAGS, [&] {
                            if (running != nullptr) {
                                const auto name = std::string { running->name };
                                Gui::collapsing(
                                  std::format("{} {}", name, running->pid),
                                  Gui::TreeNodeFlags::DefaultOpen,
                                  [&] {
                                      Gui::text("Pid: {}", running->pid);
                                      Gui::text("Arrival Time: {}", running->arrival);
                                      draw_events_table(running->events);
                                  }
                                );
                            }
                        });
                    });
                });

                if (col < cols - 1 && idx + 1 < sim->threads_count) { ImGui::SameLine(); }

                ++idx;
            }
        }
    }

    static void draw_process_queue(const std::string& title, const auto& processes, const ImVec2& child_size)
    {
        Gui::title(title, child_size, [&](const auto& remaining_size) {
            Gui::child(title, remaining_size, Gui::ChildFlags::Border, Gui::WindowFlags::AlwaysVerticalScrollbar, [&] {
                std::ranges::for_each(processes, [](const auto& process) { draw_process(process); });
            });
        });
    }

    static void draw_events_table(const Os::Process::EventsQueue& events)
    {
        constexpr static auto TABLE_NAME  = "##EventsTable";
        constexpr static auto HEADERS     = { "Event", "Duration", "Resource Usage" };
        constexpr static auto TABLE_FLAGS = Gui::TableFlags::Borders | Gui::TableFlags::RowBackground;

        if (!events.empty()) {
            Gui::draw_table(TABLE_NAME, HEADERS, TABLE_FLAGS, [&] {
                std::ranges::for_each(events, [](const auto& event) {
                    Gui::draw_table_row(
                      [&] { Gui::text("{}", event.kind); },
                      [&] { Gui::text("{}", event.duration); },
                      [&] { Gui::text("{}%", std::lround(event.resource_usage * 100)); }
                    );
                });
            });
        }
    }

    void draw_cpu_usage_graph(const ImVec2& child_size)
    {
        const auto plot_opts = Gui::Plotting::PlotOpts {
            .x_axis_flags = Gui::Plotting::AxisFlags::NoTickLabels | Gui::Plotting::AxisFlags::NoTickMarks,
            .y_axis_flags = Gui::Plotting::AxisFlags::None,
            .x_min        = delta_time - PLOT_HISTORY,
            .x_max        = delta_time,
            .y_min        = 0,
            .y_max        = 100,
            .x_label      = std::nullopt,
            .y_label      = std::nullopt,
            .color        = ImPlot::GetColormapColor(1),
            .line_weight  = 2.5F,
        };

        if (!sim->complete()) {
            cpu_usage_buffer.emplace_point(delta_time, static_cast<std::size_t>(sim->average_cpu_usage() * 100));
        }

        Gui::title("Cpu usage", child_size, [&](const auto& remaining_size) {
            Gui::Plotting::plot("##CpuUsagePlot", remaining_size, plot_opts, [&] {
                Gui::Plotting::line("cpu usage %", cpu_usage_buffer, Gui::Plotting::LineFlags::None);
            });
        });
    }

    void draw_throughput_graph(const ImVec2& child_size)
    {
        auto plot_opts = Gui::Plotting::PlotOpts {
            .x_axis_flags = Gui::Plotting::AxisFlags::NoTickLabels | Gui::Plotting::AxisFlags::NoTickMarks,
            .y_axis_flags = Gui::Plotting::AxisFlags::None,
            .x_min        = delta_time - PLOT_HISTORY,
            .x_max        = delta_time,
            .y_min        = 0,
            .y_max        = max_throughput,
            .x_label      = std::nullopt,
            .y_label      = std::nullopt,
            .color        = ImPlot::GetColormapColor(3),
            .line_weight  = 2.5F,
        };

        const auto new_value = sim->throughput;
        if (!sim->complete()) { throughput_buffer.emplace_point(delta_time, new_value); }

        Gui::title("Throughput", child_size, [&](const auto& remaining_size) {
            max_throughput  = std::max(max_throughput, new_value);
            plot_opts.y_max = max_throughput;

            Gui::Plotting::plot("##ThroughputPlot", remaining_size, plot_opts, [&] {
                Gui::Plotting::line("throughput", throughput_buffer, Gui::Plotting::LineFlags::None);
            });
        });
    }


    void draw_graphs(const ImVec2& child_size)
    {
        const auto nested_child_size = Gui::grid_layout_calc_size(2, 2, child_size);

        Gui::group([&] {
            draw_average_waiting_time_graph(nested_child_size);
            draw_average_turnaround_time_graph(nested_child_size);
        });

        ImGui::SameLine();

        Gui::group([&] {
            draw_cpu_usage_graph(nested_child_size);
            draw_throughput_graph(nested_child_size);
        });
    }

    void draw_average_waiting_time_graph(const ImVec2& child_size)
    {
        auto plot_opts = Gui::Plotting::PlotOpts {
            .x_axis_flags = Gui::Plotting::AxisFlags::NoTickLabels | Gui::Plotting::AxisFlags::NoTickMarks,
            .y_axis_flags = Gui::Plotting::AxisFlags::None,
            .x_min        = delta_time - PLOT_HISTORY,
            .x_max        = delta_time,
            .y_min        = 0,
            .y_max        = static_cast<double>(max_waiting_time),
            .x_label      = std::nullopt,
            .y_label      = std::nullopt,
            .color        = ImPlot::GetColormapColor(7),
            .line_weight  = 2.5F,
        };

        const auto new_value = sim->average_waiting_time();
        if (!sim->complete()) { average_waiting_time_buffer.emplace_point(delta_time, new_value); }

        Gui::title("Waiting time", child_size, [&](const auto& remaining_size) {
            max_waiting_time = std::max(max_waiting_time, new_value);
            plot_opts.y_max  = std::max(max_waiting_time, 1UL) + 5;

            Gui::Plotting::plot("##WaitingTimePlot", remaining_size, plot_opts, [&] {
                Gui::Plotting::line("waiting time", average_waiting_time_buffer, Gui::Plotting::LineFlags::None);
            });
        });
    }

    void draw_average_turnaround_time_graph(const ImVec2& child_size)
    {
        auto plot_opts = Gui::Plotting::PlotOpts {
            .x_axis_flags = Gui::Plotting::AxisFlags::NoTickLabels | Gui::Plotting::AxisFlags::NoTickMarks,
            .y_axis_flags = Gui::Plotting::AxisFlags::None,
            .x_min        = delta_time - PLOT_HISTORY,
            .x_max        = delta_time,
            .y_min        = 0,
            .y_max        = static_cast<double>(max_turnaround_time),
            .x_label      = std::nullopt,
            .y_label      = std::nullopt,
            .color        = ImPlot::GetColormapColor(2),
            .line_weight  = 2.5F,
        };

        const auto new_value = sim->average_turnaround_time();
        if (!sim->complete()) { average_turnaround_time_buffer.emplace_point(delta_time, new_value); }

        Gui::title("Turnaround time", child_size, [&](const auto& remaining_size) {
            max_turnaround_time = std::max(max_turnaround_time, new_value);
            plot_opts.y_max     = std::max(max_turnaround_time, 1UL) + 5;

            Gui::Plotting::plot("##TurnaroundTimePlot", remaining_size, plot_opts, [&] {
                Gui::Plotting::line("turnaround time", average_turnaround_time_buffer, Gui::Plotting::LineFlags::None);
            });
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
    float                                                   delta_time = 0.0F;
    Gui::Plotting::RingBuffer                               cpu_usage_buffer;
    Gui::Plotting::RingBuffer                               average_waiting_time_buffer;
    std::size_t                                             max_waiting_time = 0;
    Gui::Plotting::RingBuffer                               average_turnaround_time_buffer;
    std::size_t                                             max_turnaround_time = 0;
    Gui::Plotting::RingBuffer                               throughput_buffer;
    double                                                  max_throughput = 0;
};
