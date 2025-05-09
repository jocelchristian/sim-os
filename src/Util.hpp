#pragma once

#include <algorithm>
#include <charconv>
#include <print>
#include <ranges>
#include <filesystem>
#include <optional>
#include <sstream>
#include <fstream>

#if __clang__ || __GNUC__
#define TRY(failable)                     \
    ({                                    \
        auto result = (failable);         \
        if (!result) return std::nullopt; \
        *result;                          \
    })
#else
#error "Unsupported compiler: TRY macro only supported for GCC and Clang"
#endif


namespace Util
{

[[nodiscard]] constexpr static auto trim(std::string_view sv) -> std::string_view
{
    const auto not_space = [](char c) { return !std::isspace(static_cast<unsigned char>(c)); };
    sv.remove_prefix(std::ranges::distance(sv.begin(), std::ranges::find_if(sv, not_space)));
    sv.remove_suffix(std::ranges::distance(sv.rbegin(), std::ranges::find_if(sv | std::views::reverse, not_space)));

    return sv;
}


[[nodiscard]] constexpr static auto parse_number(std::string_view str) -> std::optional<std::size_t>
{
    std::size_t value = 0;
    auto [ptr, ec]    = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec != std::errc()) {
        std::println(stderr, "[ERROR] Failed to parse number from string: {}", str);
        return std::nullopt;
    }

    return value;
};

[[nodiscard]] constexpr static auto to_lower(std::string_view input) -> std::string
{
    std::string result;
    std::ranges::transform(input, std::back_inserter(result), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return result;
}

[[nodiscard]] static auto read_entire_file(const std::filesystem::path& file_path) -> std::optional<std::string>
{
    if (!std::filesystem::exists(file_path)) {
        std::println(stderr, "[ERROR] Unable to read file {}: No such file or directory", file_path.string());
        return std::nullopt;
    }

    if (!std::filesystem::is_regular_file(file_path)) {
        std::println(stderr, "[ERROR] Unable to read file {}: Not a regular file", file_path.string());
        return std::nullopt;
    }

    std::ifstream file(file_path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

} // namespace Util
