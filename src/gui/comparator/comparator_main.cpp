#include "gui/Gui.hpp"
#include "imgui.h"
#include "implot.h"
#include "Util.hpp"
#include <algorithm>
#include <cstddef>
#include <print>
#include <ranges>

[[nodiscard]] static auto split_key_value(const std::string_view line) -> std::pair<std::string_view, std::string_view>
{
    auto trim = [](std::string_view s) -> std::string_view {
        const auto first = s.find_first_not_of(" \t");
        const auto last  = s.find_last_not_of(" \t");
        return (first == std::string_view::npos) ? std::string_view {} : s.substr(first, last - first + 1);
    };

    auto parts = line | std::views::split('=') | std::views::transform([](auto&& r) {
                     return std::string_view(&*r.begin(), static_cast<std::size_t>(std::ranges::distance(r)));
                 });

    std::array<std::string_view, 2> result {};
    auto                            it = parts.begin();
    if (it != parts.end()) { result[0] = trim(*it++); }
    if (it != parts.end()) { result[1] = trim(*it); }

    return { result[0], result[1] };
}

[[nodiscard]] static auto try_parse_string_as_float(const std::string& str) -> std::optional<float>
{
    float number         = 0.0F;
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

constexpr static auto RED   = ImVec4(1.0F, 0.0F, 0.0F, 1.0F);
constexpr static auto GREEN = ImVec4(0.0F, 1.0F, 0.0F, 1.0F);

struct [[nodiscard]] ColoredValue final
{
    std::string           value;
    std::optional<ImVec4> color;
};

using ColoredTable = std::unordered_map<std::string, ColoredValue>;

[[nodiscard]] static auto assign_colors(const auto& lhs, const auto& rhs)
  -> std::optional<std::pair<ColoredTable, ColoredTable>>
{
    std::unordered_map<std::string, ColoredValue> lhs_result = {};
    std::unordered_map<std::string, ColoredValue> rhs_result = {};

    const auto lhs_keys = lhs | std::views::keys;
    const auto rhs_keys = rhs | std::views::keys;
    if (!std::ranges::equal(lhs_keys, rhs_keys)) {
        std::println("[ERROR] (parsing) the two files have different metrics. Try regenerating them");
        return std::nullopt;
    }

    for (const auto& key : lhs_keys) {
        const auto& lhs_value = lhs.at(key);
        const auto& rhs_value = rhs.at(key);

        const auto lhs_value_number = try_parse_string_as_float(lhs_value);
        const auto rhs_value_number = try_parse_string_as_float(rhs_value);
        if (!lhs_value_number || !rhs_value_number) {
            lhs_result.emplace(key, ColoredValue { .value = lhs_value, .color = std::nullopt });
            rhs_result.emplace(key, ColoredValue { .value = rhs_value, .color = std::nullopt });
            continue;
        }

        constexpr static auto LOWER_BETTER = {
            "avg_waiting_time", "max_waiting_time", "avg_turnaround_time", "max_turnaround_time", "timer"
        };
        if (lhs_value_number.value() < rhs_value_number.value()) {
            if (std::ranges::contains(LOWER_BETTER, key)) {
                lhs_result.emplace(key, ColoredValue { .value = lhs_value, .color = GREEN });
                rhs_result.emplace(key, ColoredValue { .value = rhs_value, .color = RED });
            } else {
                lhs_result.emplace(key, ColoredValue { .value = lhs_value, .color = RED });
                rhs_result.emplace(key, ColoredValue { .value = rhs_value, .color = GREEN });
            }
        } else {
            if (std::ranges::contains(LOWER_BETTER, key)) {
                lhs_result.emplace(key, ColoredValue { .value = lhs_value, .color = RED });
                rhs_result.emplace(key, ColoredValue { .value = rhs_value, .color = GREEN });
            } else {
                lhs_result.emplace(key, ColoredValue { .value = lhs_value, .color = GREEN });
                rhs_result.emplace(key, ColoredValue { .value = rhs_value, .color = RED });
            }
        }
    }

    return std::make_optional(std::make_pair(lhs_result, rhs_result));
}

[[maybe_unused]] static void draw_metrics_table(const std::string& name, const ColoredTable& table)
{
    const auto HEADERS = { "Key", "Value" };
    Gui::draw_table(
      std::format("{}Table", name),
      HEADERS,
      Gui::TableFlags::Borders | Gui::TableFlags::RowBackground,
      [&] {
          for (const auto& [key, value] : table) {
              if (value.color.has_value()) { ImGui::PushStyleColor(ImGuiCol_Text, value.color.value()); }
              Gui::draw_table_row([&] { Gui::text("{}", key); }, [&] { Gui::text("{}", value.value); });
              if (value.color.has_value()) { ImGui::PopStyleColor(); }
          }
      }
    );
}

static void usage(const char* executable) { std::println("{}: <file1.met> <file2.met>", executable); }

auto main(int argc, const char** argv) -> int
{
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const std::filesystem::path& first_file  = argv[1];
    const std::filesystem::path& second_file = argv[2];

    const auto first_content  = Util::read_entire_file(first_file);
    const auto second_content = Util::read_entire_file(second_file);
    if (!first_content || !second_content) { return 1; }

    const auto first_table  = parse_content(first_content.value());
    const auto second_table = parse_content(second_content.value());
    if (!first_table || !second_table) { return 1; }

    const auto result = assign_colors(first_table.value(), second_table.value());
    if (!result) { return 1; }
    const auto [first_table_colored, second_table_colored] = result.value();

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
                          .x_axis_flags = Gui::Plotting::AxisFlags::AutoFit,
                          .y_axis_flags = Gui::Plotting::AxisFlags::AutoFit,
                          .x_min        = -0.5,
                          .x_max        = 2 - 0.5,
                          .y_min        = std::nullopt,
                          .y_max        = std::nullopt,
                          .x_label      = std::nullopt,
                          .y_label      = std::nullopt,
                          .color        = std::nullopt,
                          .line_weight  = std::nullopt,
                          .can_scroll   = false,

                      };
                      constexpr static auto TO_IGNORE = { "schedule_policy" };
                      auto keys = std::views::filter(first_table_colored | std::views::keys, [](const auto& elem) {
                          return !std::ranges::contains(TO_IGNORE, elem);
                      });

                      const auto keys_count = std::ranges::distance(keys);
                      const auto cols       = static_cast<int>(std::ceil(std::sqrt(keys_count)));
                      const auto rows =
                        static_cast<int>(std::ceil(static_cast<double>(keys_count) / static_cast<double>(cols)));
                      // const auto plot_size = Gui::grid_layout_calc_size(
                      // static_cast<std::size_t>(rows), static_cast<std::size_t>(cols), ImGui::GetContentRegionAvail()
                      // );

                      ImPlot::BeginSubplots("##HistogramSubplots", rows, cols, ImGui::GetContentRegionAvail(), 0);

                      for (const auto& key : keys) {
                          const auto raw_values =
                            std::array { first_table_colored.at(key).value, second_table_colored.at(key).value };

                          std::array<double, raw_values.size()> values {};
                          for (const auto& [idx, elem] : std::views::zip(std::views::iota(0UL), raw_values)) {
                              values[idx] = try_parse_string_as_float(elem).value();
                          }

                          const auto max_value = std::ranges::max(values);
                          plot_opts.y_max      = max_value * 1.1;

                          Gui::Plotting::plot(key, ImVec2(0, 0), plot_opts, [&] {
                              ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                              std::array<std::string, 2> labels {};
                              for (std::size_t idx = 0; idx < 2; ++idx) {
                                  labels[idx] = std::format("{} #{}", key, idx);
                              }

                              std::array<const char*, 2> labels_cstr {};
                              for (std::size_t idx = 0; idx < 2; ++idx) { labels_cstr[idx] = labels[idx].c_str(); }

                              std::array<double, 2> positions {};
                              for (std::size_t pos = 0; pos < 2; ++pos) { positions[pos] = static_cast<double>(pos); }

                              constexpr static auto BAR_WIDTH = 0.2F;
                              ImPlot::SetupAxisTicks(ImAxis_X1, positions.data(), positions.size(), labels_cstr.data());

                              const auto coordinates = std::views::zip(positions, values);
                              for (const auto& [idx, coords] : std::views::zip(std::views::iota(0), coordinates)) {
                                  const auto& [x, y] = coords;
                                  ImPlot::PushStyleColor(
                                    ImPlotCol_Fill, ImPlot::GetColormapColor(idx % ImPlot::GetColormapSize())
                                  );
                                  ImPlot::PlotBars(std::format("{} #{}", key, idx).c_str(), &x, &y, 1, BAR_WIDTH);
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
