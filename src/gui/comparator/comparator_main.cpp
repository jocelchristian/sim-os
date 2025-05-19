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

[[nodiscard]] static auto parse_content(const std::string& content)
  -> std::optional<std::unordered_map<std::string, std::string>>
{
    std::unordered_map<std::string, std::string> result = {};

    for (const auto& line_range : content | std::views::split('\n')) {
        const auto line = std::string_view { line_range };
        if (line == "separator") { continue; }

        const auto [key, value] = split_key_value(line);
        if (key.empty() && value.empty()) { continue; }
        result.emplace(key, value);
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

    const auto file_contents = read_files(file_paths);
    if (!file_contents || file_contents->size() < 2) { return 1; }

    const auto tables = tables_from_file_contents(file_contents.value());
    if (!tables) { return 1; }

    const auto maybe_window = Gui::init_window("sim-os: comparator", 1920, 1080);
    if (!maybe_window) { return 1; }
    auto* window = maybe_window.value();

    ImPlot::CreateContext();

    auto& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = io.Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14.0F);

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
          [&] {
              Gui::group([&] {
                  Gui::child("##HistogramChild", Gui::ChildFlags::Border, Gui::WindowFlags::None, [&] {
                      Gui::Plotting::PlotOpts plot_opts = {
                          .x_axis_flags = Gui::Plotting::AxisFlags::AutoFit | Gui::Plotting::AxisFlags::NoTickLabels
                                          | Gui::Plotting::AxisFlags::NoTickMarks,
                          .y_axis_flags = Gui::Plotting::AxisFlags::AutoFit,
                          .x_min        = -0.5,
                          .x_max        = static_cast<double>(tables->size()) - 0.5,
                          .y_min        = std::nullopt,
                          .y_max        = std::nullopt,
                          .x_label      = std::nullopt,
                          .y_label      = std::nullopt,
                          .color        = std::nullopt,
                          .line_weight  = std::nullopt,
                          .can_scroll   = false,

                      };
                      constexpr static auto TO_IGNORE = { "schedule_policy" };
                      auto keys = std::views::filter(tables.value().front() | std::views::keys, [](const auto& elem) {
                          return !std::ranges::contains(TO_IGNORE, elem);
                      });

                      const auto keys_count = std::ranges::distance(keys);
                      const auto cols       = static_cast<int>(std::ceil(std::sqrt(keys_count)));
                      const auto rows =
                        static_cast<int>(std::ceil(static_cast<double>(keys_count) / static_cast<double>(cols)));

                      ImPlot::BeginSubplots("##HistogramSubplots", rows, cols, ImGui::GetContentRegionAvail(), 0);

                      for (const auto& key : keys) {
                          std::vector<double> values;
                          for (const auto& table : *tables) {
                              const auto parse_result = try_parse_string_as_double(table.at(key));
                              assert(parse_result.has_value() && "unreachable");
                              values.push_back(parse_result.value());
                          }

                          plot_opts.y_max = std::ranges::max(values) * 1.1;

                          Gui::Plotting::plot(key, ImVec2(0, 0), plot_opts, [&] {
                              ImPlot::SetupLegend(ImPlotLocation_NorthWest);

                              std::vector<std::string> labels;
                              for (const auto& file_path : file_paths) { labels.push_back(file_path.stem().string()); }

                              std::vector<const char*> labels_cstr;
                              for (const auto& label : labels) { labels_cstr.push_back(label.c_str()); }

                              std::vector<double> positions {};
                              positions.reserve(tables->size());
                              for (std::size_t pos = 0; pos < tables->size(); ++pos) {
                                  positions.push_back(static_cast<double>(pos));
                              }

                              constexpr static auto BAR_WIDTH = 0.2F;
                              ImPlot::SetupAxisTicks(
                                ImAxis_X1, positions.data(), static_cast<int>(positions.size()), nullptr
                              );

                              const auto coordinates = std::views::zip(positions, values);
                              for (const auto& [idx, coords] : std::views::zip(std::views::iota(0UL), coordinates)) {
                                  const auto& [x, y] = coords;
                                  ImPlot::PushStyleColor(
                                    ImPlotCol_Fill,
                                    ImPlot::GetColormapColor(static_cast<int>(idx) % ImPlot::GetColormapSize())
                                  );
                                  ImPlot::PlotBars(labels_cstr[idx], &x, &y, 1, BAR_WIDTH);
                                  ImPlot::PopStyleColor();
                              }
                          });
                      }

                      ImPlot::EndSubplots();
                  });
              });
          }
        );

        Gui::draw_call(window, ImVec4(.96F, .96F, .96F, 1.0F));
    }

    ImPlot::DestroyContext();
    Gui::shutdown(window);
}
