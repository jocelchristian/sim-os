#include "Application.hpp"

#include <numeric>

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

static void draw_process(const auto& process)
{
    if (process == nullptr) { return; }

    auto header_title = std::format("{} #{}", process->name, process->pid);
    Gui::collapsing(header_title, Gui::TreeNodeFlags::DefaultOpen, [&] {
        if (process->name != "Process") { header_title = std::string { process->name }; }
        Gui::text("Pid: {}", process->pid);
        Gui::text("Arrival Time: {}", process->arrival);
        draw_events_table(process->events);
    });
}

static void draw_process_queue(const std::string& title, const auto& processes, const ImVec2& child_size)
{
    Gui::title(title, child_size, [&] {
        std::ranges::for_each(processes, [](const auto& process) { draw_process(process); });
    });
}

auto Application::create(const std::shared_ptr<Simulations::Scheduler>& sim) -> std::unique_ptr<Application>
{
    const auto window = Gui::init_window("sim-os: scheduler", WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!window) { return nullptr; }
    ImPlot::CreateContext();

    Gui::load_default_fonts();
    Gui::black_and_red_style();

    return std::unique_ptr<Application>(new Application { *window, sim });
}

void Application::render()
{
    while (!quit) {
        stepped_this_frame = false;
        if (glfwWindowShouldClose(window) == 1) { quit = true; }

        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) { should_finish = !should_finish; }

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

        if (!sim->complete() && stepped_this_frame) { delta_time += ImGui::GetIO().DeltaTime; }

        Gui::new_frame();

        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        Gui::window(
          "sim-os: scheduler",
          Gui::WindowFlags::NoDecoration | Gui::WindowFlags::NoResize | Gui::WindowFlags::NoMove,
          [this] {
              draw_save_button();

              ImGui::SameLine();

              draw_control_buttons();

              ImGui::SameLine();

              draw_scheduler_policy_picker();

              const std::array<Gui::IndexGridCallback, 6> drawables = {
                  [&](const auto& size) { draw_process_queue("Ready", std::views::join(sim->ready), size); },
                  [&](const auto& size) { draw_process_queue("Waiting", std::views::join(sim->waiting), size); },
                  [&](const auto& size) { draw_running_process(size); },
                  [&](const auto& size) { draw_process_queue("Arrival", std::views::join(sim->processes), size); },
                  [&](const auto& size) { draw_graphs(size); },
                  [&](const auto& size) { draw_statistics(size); }
              };

              const auto available_space = ImGui::GetContentRegionAvail();
              Gui::grid(2UL, 3UL, 6UL, available_space, [&](const auto& child_size, const auto& idx) {
                  drawables[idx](child_size);
              });
          }
        );

        Gui::draw_call(window, BACKGROUND_COLOR);
    }
}

void Application::draw_save_button() const
{
    static bool show_input_box = false;

    if (show_input_box) {
        const auto file_path = Gui::input_text_popup("Enter file path: ", show_input_box);
        if (!file_path.has_value()) {
            return;
        } else if (file_path->empty()) {
            Gui::toast(
              std::format("Failed to save simulation to {}: invalid path", file_path.value()),
              Gui::ToastPosition::BottomRight,
              std::chrono::seconds(3),
              Gui::ToastLevel::Error
            );
            return;
        }

        std::stringstream ss;
        ss << std::format("timer = {}\n", sim->timer);
        ss << std::format("schedule_policy = {}\n", sim->schedule_policy.name());

        ss << "separator\n";

        ss << std::format("avg_waiting_time = {}\n", sim->average_waiting_time());
        ss << std::format("max_waiting_time = {}\n", max_waiting_time);
        ss << std::format("avg_turnaround_time = {}\n", sim->average_turnaround_time());
        ss << std::format("max_turnaround_time = {}\n", max_turnaround_time);
        ss << std::format("avg_throughput = {:.2f}\n", sim->throughput);
        ss << std::format("max_throughput = {:.2f}\n", max_throughput);

        Util::write_to_file(file_path.value(), ss.str());
        Gui::toast(
          std::format("Saved simulation result to {}", file_path.value()),
          Gui::ToastPosition::BottomRight,
          std::chrono::seconds(2),
          Gui::ToastLevel::Info
        );
    }

    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) { show_input_box = true; }

    Gui::enabled_if(sim->complete(), [&] {
        Gui::image_button(save_texture, BUTTON_SIZE, "[Ctrl+S]ave Results", [&] { show_input_box = true; });
    });
}

void Application::draw_control_buttons()
{
    constexpr static auto BUTTONS_COUNT = 3;
    Gui::center_content_horizontally(BUTTON_SIZE.x * BUTTONS_COUNT);

    const auto restart_callback = [this] {
        sim->restart();
        should_finish      = false;
        stepped_this_frame = false;
        delta_time         = 0.0F;
        cpu_usage_buffer.clear();
        average_waiting_time_buffer.clear();
        max_waiting_time = 0;
        average_turnaround_time_buffer.clear();
        max_turnaround_time = 0;
        cpu_usage_buffer.clear();
        throughput_buffer.clear();
        max_throughput = 0;
    };

    Gui::enabled_if(sim->complete(), [&] {
        Gui::image_button(restart_texture, BUTTON_SIZE, "[Ctrl+R]estart", restart_callback);
    });
    if (sim->complete() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R, false)) { restart_callback(); }

    ImGui::SameLine();

    Gui::image_button(play_texture, BUTTON_SIZE, "[Enter] Play", [this] {
        if (!sim->complete()) { should_finish = !should_finish; }
    });

    ImGui::SameLine();

    Gui::image_button(next_texture, BUTTON_SIZE, "[Space] Next", [this] {
        if (!sim->complete()) { sim->step(); }
    });
}

void Application::draw_scheduler_policy_picker()
{
    // FIXME: Mind that the order here matters, you have to declare these in the same way they are
    // declared in the enum. Is kinda sus but i don't see how to fix this rn.
    constexpr static auto ITEMS =
      std::array<Simulations::SchedulePolicy, 2> { Simulations::SchedulePolicy::FirstComeFirstServed,
                                                   Simulations::SchedulePolicy::RoundRobin };

    Gui::combo(
      "##SchedulePolicyPicker",
      std::span(ITEMS.begin(), ITEMS.end()),
      sim->schedule_policy.kind(),
      [&](const auto& selected) { sim->switch_schedule_policy(Simulations::named_scheduler_from_policy(selected)); }
    );
}


void Application::draw_running_process(const ImVec2& child_size) const
{
    Gui::grid(sim->threads_count, child_size, [&](const auto& elem_size, const auto& idx) {
        const auto running = sim->running[idx];
        const auto title   = std::format("CPU Core #{}", idx);

        Gui::title(title, elem_size, [&] {
            if (running != nullptr) {
                const auto name = std::string { running->name };
                Gui::collapsing(std::format("{} {}", name, running->pid), Gui::TreeNodeFlags::DefaultOpen, [&] {
                    Gui::text("Pid: {}", running->pid);
                    Gui::text("Arrival Time: {}", running->arrival);
                    draw_events_table(running->events);
                });
            }
        });
    });
}

void Application::draw_graphs(const ImVec2& child_size)
{
    const std::array<Gui::IndexGridCallback, 4> callbacks = {
        [&](const auto& elem_size) { draw_average_waiting_time_graph(elem_size); },
        [&](const auto& elem_size) { draw_average_turnaround_time_graph(elem_size); },
        [&](const auto& elem_size) { draw_cpu_usage_graph(elem_size); },
        [&](const auto& elem_size) { draw_throughput_graph(elem_size); },
    };

    Gui::grid(2UL, 2UL, 4UL, child_size, [&](const auto& elem_size, const auto& idx) { callbacks[idx](elem_size); });
}

void Application::draw_cpu_usage_graph(const ImVec2& child_size)
{
    const auto plot_opts = Gui::Plotting::PlotOpts {
        .x_axis_flags = Gui::Plotting::AxisFlags::NoTickLabels | Gui::Plotting::AxisFlags::NoTickMarks,
        .y_axis_flags = Gui::Plotting::AxisFlags::None,
        .x_min        = delta_time - PLOT_HISTORY,
        .x_max        = delta_time,
        .y_min        = 0,
        .y_max        = 100,
        .color        = ImPlot::GetColormapColor(1),
        .line_weight  = 2.5F,
        .scrollable   = sim->complete(),
    };

    if (!sim->complete()) {
        cpu_usage_buffer.emplace_point(delta_time, static_cast<float>(sim->average_cpu_usage() * 100));
    }

    Gui::title("Cpu usage", child_size, [&](const auto& remaining_size) {
        Gui::Plotting::plot("##CpuUsagePlot", remaining_size, plot_opts, [&] {
            Gui::Plotting::line("cpu usage %", cpu_usage_buffer, Gui::Plotting::LineFlags::None);
        });
    });
}

void Application::draw_throughput_graph(const ImVec2& child_size)
{
    auto plot_opts = Gui::Plotting::PlotOpts {
        .x_axis_flags = Gui::Plotting::AxisFlags::NoTickLabels | Gui::Plotting::AxisFlags::NoTickMarks,
        .y_axis_flags = Gui::Plotting::AxisFlags::None,
        .x_min        = delta_time - PLOT_HISTORY,
        .x_max        = delta_time,
        .y_min        = 0,
        .y_max        = max_throughput,
        .color        = ImPlot::GetColormapColor(3),
        .line_weight  = 2.5F,
        .scrollable   = sim->complete(),
    };

    const auto new_value = sim->throughput;
    if (!sim->complete()) { throughput_buffer.emplace_point(delta_time, static_cast<float>(new_value)); }

    Gui::title("Throughput", child_size, [&](const auto& remaining_size) {
        max_throughput  = std::max(max_throughput, new_value);
        plot_opts.y_max = max_throughput;

        Gui::Plotting::plot("##ThroughputPlot", remaining_size, plot_opts, [&] {
            Gui::Plotting::line("throughput", throughput_buffer, Gui::Plotting::LineFlags::None);
        });
    });
}

void Application::draw_average_waiting_time_graph(const ImVec2& child_size)
{
    auto plot_opts = Gui::Plotting::PlotOpts {
        .x_axis_flags = Gui::Plotting::AxisFlags::NoTickLabels | Gui::Plotting::AxisFlags::NoTickMarks,
        .y_axis_flags = Gui::Plotting::AxisFlags::None,
        .x_min        = delta_time - PLOT_HISTORY,
        .x_max        = delta_time,
        .y_min        = 0,
        .y_max        = static_cast<double>(max_waiting_time),
        .color        = ImPlot::GetColormapColor(7),
        .line_weight  = 2.5F,
        .scrollable   = sim->complete(),
    };

    const auto new_value = sim->average_waiting_time();
    if (!sim->complete()) { average_waiting_time_buffer.emplace_point(delta_time, static_cast<float>(new_value)); }

    Gui::title("Waiting time", child_size, [&](const auto& remaining_size) {
        max_waiting_time = std::max(max_waiting_time, new_value);
        plot_opts.y_max  = static_cast<double>(std::max(max_waiting_time, 1UL) + 5);

        Gui::Plotting::plot("##WaitingTimePlot", remaining_size, plot_opts, [&] {
            Gui::Plotting::line("waiting time", average_waiting_time_buffer, Gui::Plotting::LineFlags::None);
        });
    });
}

void Application::draw_average_turnaround_time_graph(const ImVec2& child_size)
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
        .scrollable   = sim->complete(),
    };

    const auto new_value = sim->average_turnaround_time();
    if (!sim->complete()) { average_turnaround_time_buffer.emplace_point(delta_time, static_cast<float>(new_value)); }

    Gui::title("Turnaround time", child_size, [&](const auto& remaining_size) {
        max_turnaround_time = std::max(max_turnaround_time, new_value);
        plot_opts.y_max     = static_cast<double>(std::max(max_turnaround_time, 1UL) + 5);

        Gui::Plotting::plot("##TurnaroundTimePlot", remaining_size, plot_opts, [&] {
            Gui::Plotting::line("turnaround time", average_turnaround_time_buffer, Gui::Plotting::LineFlags::None);
        });
    });
}

void Application::draw_statistics(const ImVec2& child_size) const
{
    constexpr static auto TABLE_FLAGS = Gui::TableFlags::Borders | Gui::TableFlags::RowBackground;

    Gui::title("Stats", child_size, [&] {
        constexpr static auto INFO_TABLE_HEADERS = { "Key", "Value" };
        Gui::draw_table("InfoTable", INFO_TABLE_HEADERS, TABLE_FLAGS, [&] {
            const auto draw_key_value = [](const std::string_view key, const auto& value) {
                Gui::draw_table_row([&] { Gui::text("{}", key); }, [&] { Gui::text("{}", value); });
            };

            draw_key_value("Timer", sim->timer);
            draw_key_value("Scheduler Policy", sim->schedule_policy.name());
        });

        ImGui::Separator();

        constexpr static auto QUEUES_TABLE_HEADERS = { "Queue", "Size" };
        Gui::draw_table("QueuesTable", QUEUES_TABLE_HEADERS, TABLE_FLAGS, [&] {
            const auto draw_key_value = [](const std::string_view key, const auto& value) {
                Gui::draw_table_row([&] { Gui::text("{}", key); }, [&] { Gui::text("{}", value); });
            };

            const auto calculate_size = [](const auto& queues) -> std::size_t {
                return std::accumulate(queues.begin(), queues.end(), 0UL, [](const auto& acc, const auto& queue) {
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
                  std::format("Core #{}", thread_idx), static_cast<std::size_t>(sim->cpu_usage[thread_idx] * 100)
                );
            }
        });

        ImGui::Separator();

        constexpr static auto METRICS_TABLE_HEADERS = { "Key", "Value" };
        Gui::draw_table("MetricsTable", METRICS_TABLE_HEADERS, TABLE_FLAGS, [&] {
            const auto draw_key_value = [](const std::string_view key, const auto& value) {
                Gui::draw_table_row(
                  [&] { Gui::text("{}", key); },
                  [&] {
                      using Type = std::decay_t<decltype(value)>;
                      if constexpr (std::is_same_v<Type, double>) {
                          Gui::text("{:.2f}", value);
                      } else {
                          Gui::text("{}", value);
                      }
                  }
                );
            };

            draw_key_value("Avg. waiting time", sim->average_waiting_time());
            draw_key_value("Max. waiting time", max_waiting_time);
            draw_key_value("Avg. turnaround time", sim->average_turnaround_time());
            draw_key_value("Max. turnaround time", max_turnaround_time);
            draw_key_value("Avg. throughput", sim->throughput);
            draw_key_value("Max. throughput", max_throughput);
        });
    });
}

Application::Application(GLFWwindow* window, const std::shared_ptr<Simulations::Scheduler>& sim)
  : window { window },
    sim { sim },
    restart_texture { Gui::Texture::load_from_file("resources/restart.png") },
    play_texture { Gui::Texture::load_from_file("resources/play.png") },
    next_texture { Gui::Texture::load_from_file("resources/next.png") },
    save_texture { Gui::Texture::load_from_file("resources/save.png") }
{}

Application::~Application()
{
    Gui::shutdown(window);
    ImPlot::DestroyContext();
}
