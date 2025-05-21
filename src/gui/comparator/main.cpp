#include <cstddef>
#include <print>
#include <ranges>
#include <unordered_map>

#include "Application.hpp"
#include "Util.hpp"

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

[[nodiscard]] static auto parse_content(const std::string& content)
  -> std::optional<std::unordered_map<std::string, std::string>>
{
    std::unordered_map<std::string, std::string> result = {};

    for (const auto& line_range : content | std::views::split('\n')) {
        const auto line = std::string_view { line_range };
        if (line == "separator") { continue; }

        const auto [key, value] = split_key_value(line);
        if (key.empty() && value.empty()) { continue; }
        result.emplace(Util::capitalize(Util::wordify(std::string(key))), value);
    }

    return result;
}

[[nodiscard]] static auto tables_from_file_contents(const std::span<const std::string> contents)
  -> std::optional<std::vector<std::unordered_map<std::string, std::string>>>
{
    std::vector<std::unordered_map<std::string, std::string>> tables;
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

    std::vector<std::string> file_stems;
    for (const auto& file_path : file_paths) { file_stems.push_back(file_path.stem().string()); }

    const auto file_contents = read_files(file_paths);
    if (!file_contents || file_contents->size() < 2) { return 1; }

    const auto tables = tables_from_file_contents(file_contents.value());
    if (!tables) { return 1; }

    const auto app = Application::create(file_stems, tables.value());
    if (!app) { return 1; }
    app->render();
}
