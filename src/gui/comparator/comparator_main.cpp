#include "gui/Gui.hpp"
#include "imgui.h"
#include "implot.h"
#include "Util.hpp"
#include <algorithm>
#include <cstddef>
#include <print>
#include <ranges>
#include <unordered_map>

[[nodiscard]] static auto split_key_value(const std::string_view line) -> std::pair<std::string_view, std::string_view>
{
    auto parts = line | std::views::split('=') | std::views::transform([](auto&& r) {
                     return std::string_view(&*r.begin(), static_cast<std::size_t>(std::ranges::distance(r)));
                 });

    std::array<std::string_view, 2> result {};
    auto                            it = parts.begin();
    if (it != parts.end()) { result[0] = Util::trim(*it++); }
    if (it != parts.end()) { result[1] = Util::trim(*it); }

    return { result[0], result[1] };
}

[[nodiscard]] static auto try_parse_string_as_double(const std::string& str) -> std::optional<double>
{
    double number        = 0.0F;
    const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), number);
    if (ec != std::errc {}) { return std::nullopt; }

    return number;
}

[[nodiscard]] static auto wordify(std::string str) -> std::string
{
    std::ranges::replace(str, '_', ' ');
    return str;
}

[[nodiscard]] static auto capitalize(std::string str) -> std::string
{
    for (auto word : str | std::views::split(' ')) {
        if (!word.empty()) {
            auto first = word.begin();
            *first     = static_cast<char>(std::toupper(static_cast<unsigned char>(*first)));
        }
    }

    return str;
}

[[nodiscard]] static auto parse_content(const std::string& content)
  -> std::optional<std::unordered_map<std::string, std::string>>
{
    std::unordered_map<std::string, std::string> result = {};

    for (const auto& line_range : content | std::views::split('\n')) {
        const auto line = std::string_view { line_range };
        if (line == "separator") { continue; }

        const auto [key, value] = split_key_value(line);
        if (key.empty() && value.empty()) { continue; }
        result.emplace(capitalize(wordify(std::string(key))), value);
    }

    return result;
}

[[nodiscard]] static auto read_files(const std::span<const std::filesystem::path> paths)
  -> std::optional<std::vector<std::string>>
{
    std::vector<std::string> file_contents;
    file_contents.reserve(paths.size());
    for (const auto& path : paths) {
        const auto content = TRY(Util::read_entire_file(path));
        file_contents.push_back(content);
    }

    return file_contents;
}

using Table = std::unordered_map<std::string, std::string>;

[[nodiscard]] static auto tables_from_file_contents(const std::span<const std::string> contents)
  -> std::optional<std::vector<Table>>
{
    std::vector<Table> tables;
    tables.reserve(contents.size());
    for (const auto& file_content : contents) {
        const auto parsed_content = TRY(parse_content(file_content));
        tables.push_back(parsed_content);
    }

    return tables;
}

[[nodiscard]] static auto valid_keys(const auto keys) -> std::vector<std::string>
{
    constexpr static auto TO_IGNORE = { "Schedule Policy" };

    // clang-format off
    return keys
        | std::views::filter([&](const auto& elem) { return !std::ranges::contains(TO_IGNORE, elem); })
        | std::ranges::to<std::vector<std::string>>();
    // clang-format on
}

[[nodiscard]] static auto group_tables_by_keys(const std::span<const Table> tables)
  -> std::unordered_map<std::string, std::vector<double>>
{
    std::unordered_map<std::string, std::vector<double>> result;

    const auto keys = valid_keys(tables.front() | std::views::keys);
    for (const auto& key : keys) {
        std::vector<double> values;
        for (const auto& table : tables) {
            const auto parse_result = try_parse_string_as_double(table.at(key));
            assert(parse_result.has_value() && "unreachable");
            values.push_back(parse_result.value());
        }

        result.emplace(key, values);
    }

    return result;
}

static void draw_bar_charts(const std::span<const std::string> labels, const auto& values)
{
    const auto keys = valid_keys(values | std::views::keys);

    auto plot_opts = Gui::Plotting::PlotOpts {
        .x_axis_flags = Gui::Plotting::AxisFlags::AutoFit | Gui::Plotting::AxisFlags::NoTickLabels
                        | Gui::Plotting::AxisFlags::NoTickMarks,
        .y_axis_flags = Gui::Plotting::AxisFlags::AutoFit,
        .x_min        = -0.5,
        .x_max        = static_cast<double>(labels.size()) - 0.5,
        .scrollable   = false,
        .maximizable  = false,
    };

    ImPlot::PushStyleColor(ImPlotCol_FrameBg, Gui::hex_colour_to_imvec4(0x181818));

    Gui::grid(keys.size(), ImGui::GetContentRegionAvail(), [&](const auto& subplot_size, const auto& idx) {
        const auto& key = keys[idx];
        plot_opts.y_max = std::ranges::max(values.at(key)) * 1.1;
        Gui::title(key, subplot_size, [&] {
            Gui::Plotting::plot(std::format("##{}", key), ImGui::GetContentRegionAvail(), plot_opts, [&] {
                Gui::Plotting::bars(labels, values.at(key));
            });
        });
    });

    ImPlot::PopStyleColor();
}

static void usage(const char* executable) { std::println("{}: (<file1.met> <file2.met>)+", executable); }

auto main(int argc, const char** argv) -> int
{
    const std::span args(argv, static_cast<std::size_t>(argc));
    if (args.size() < 3) {
        usage(args[0]);
        return 1;
    }

    std::vector<std::filesystem::path> file_paths;
    for (const auto& arg : args | std::views::drop(1)) { file_paths.emplace_back(arg); }

    std::vector<std::string> file_stems;
    for (const auto& file_path : file_paths) { file_stems.push_back(file_path.stem().string()); }

    const auto file_contents = read_files(file_paths);
    if (!file_contents || file_contents->size() < 2) { return 1; }

    const auto tables = tables_from_file_contents(file_contents.value());
    if (!tables) { return 1; }

    const auto grouped = group_tables_by_keys(tables.value());

    const auto maybe_window = Gui::init_window("sim-os: comparator", 1920, 1080);
    if (!maybe_window) { return 1; }
    auto* window = maybe_window.value();

    ImPlot::CreateContext();

    Gui::load_default_fonts();
    Gui::black_and_red_style();

    bool quit = false;
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
          [&] { draw_bar_charts(file_stems, grouped); }
        );

        Gui::draw_call(window, Gui::hex_colour_to_imvec4(0x181818));
    }

    ImPlot::DestroyContext();
    Gui::shutdown(window);
}
