#include "Application.hpp"

#include <algorithm>
#include <ranges>

#include "Util.hpp"

[[nodiscard]] static auto valid_keys(const auto keys) -> std::vector<std::string>
{
    constexpr static auto TO_IGNORE = { "Schedule Policy" };

    // clang-format off
    return keys
        | std::views::filter([&](const auto& elem) { return !std::ranges::contains(TO_IGNORE, elem); })
        | std::ranges::to<std::vector<std::string>>();
    // clang-format on
}

[[nodiscard]] static auto group_tables_by_keys(const std::span<const std::unordered_map<std::string, std::string>>& tables)
  -> std::unordered_map<std::string, std::vector<double>>
{
    std::unordered_map<std::string, std::vector<double>> result;

    const auto keys = valid_keys(tables.front() | std::views::keys);
    for (const auto& key : keys) {
        std::vector<double> values;
        for (const auto& table : tables) {
            const auto parse_result = Util::parse_double(table.at(key));
            assert(parse_result.has_value() && "unreachable");
            values.push_back(parse_result.value());
        }

        result.emplace(key, values);
    }

    return result;
}

auto Application::create(
  const std::span<const std::string>                             labels,
  const std::span<const std::unordered_map<std::string, std::string>>& values
) -> std::unique_ptr<Application>
{
    const auto window = Gui::init_window("sim-os: comparator", WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!window) { return nullptr; }
    ImPlot::CreateContext();

    Gui::load_default_fonts();
    Gui::black_and_red_style();

    return std::unique_ptr<Application>(new Application { *window, labels, group_tables_by_keys(values) });
}

void Application::render()
{
    while (!quit) {
        if (glfwWindowShouldClose(window) == 1) { quit = true; }

        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) { continue; }

        Gui::new_frame();

        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        Gui::window(
          "sim-os: comparator",
          Gui::WindowFlags::NoDecoration | Gui::WindowFlags::NoResize | Gui::WindowFlags::NoMove,
          [&] { draw_bar_charts(); }
        );

        Gui::draw_call(window, Gui::hex_colour_to_imvec4(0x181818));
    }
}

void Application::draw_bar_charts() const
{
    const auto keys = valid_keys(tables | std::views::keys);

    auto plot_opts = Gui::Plotting::PlotOpts {
        .x_axis_flags = Gui::Plotting::AxisFlags::AutoFit | Gui::Plotting::AxisFlags::NoTickLabels
                        | Gui::Plotting::AxisFlags::NoTickMarks,
        .y_axis_flags = Gui::Plotting::AxisFlags::AutoFit,
        .x_min        = -0.5,
        .x_max        = static_cast<double>(labels.size()) - 0.5,
        .scrollable   = false,
    };

    Gui::grid(keys.size(), ImGui::GetContentRegionAvail(), [&](const auto& subplot_size, const auto& idx) {
        const auto& key = keys[idx];
        plot_opts.y_max = std::ranges::max(tables.at(key)) * 1.1;
        Gui::title(key, subplot_size, [&] {
            Gui::Plotting::plot(std::format("##{}", key), ImGui::GetContentRegionAvail(), plot_opts, [&] {
                Gui::Plotting::bars(labels, tables.at(key));
            });
        });
    });
}

Application::~Application()
{
    ImPlot::DestroyContext();
    Gui::shutdown(window);
}

Application::Application(
  GLFWwindow*                                                 window,
  const std::span<const std::string>                          labels,
  const std::unordered_map<std::string, std::vector<double>>& tables
)
  : window { window },
    labels { labels },
    tables { tables }
{}
