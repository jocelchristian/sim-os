#pragma once

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <optional>
#include <print>
#include <ranges>

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

template<typename... Lambdas>
struct [[nodiscard]] Visitor : public Lambdas...
{
    using Lambdas::operator()...;
};

template<typename... Lambdas>
[[nodiscard]] constexpr static auto make_visitor(Lambdas... lambdas) -> Visitor<Lambdas...>
{
    return Visitor { lambdas... };
}

template<typename Alternative>
[[nodiscard]] auto get(const auto& variant) -> std::optional<Alternative>
{
    if (const auto* value = std::get_if<Alternative>(&variant)) { return *value; }

    return std::nullopt;
}

[[nodiscard]] auto read_entire_file(const std::filesystem::path& file_path) -> std::optional<std::string>;
void               write_to_file(const std::filesystem::path& file_path, const std::string& content);

[[nodiscard]] auto random_float() -> float;
[[nodiscard]] auto random_natural(const std::size_t min, const std::size_t max) -> std::size_t;


[[nodiscard]] constexpr static auto parse_double(const std::string& str) -> std::optional<double>
{
    double number        = 0.0F;
    const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), number);
    if (ec != std::errc {}) { return std::nullopt; }

    return number;
}

[[nodiscard]] constexpr static auto wordify(std::string str) -> std::string
{
    std::ranges::replace(str, '_', ' ');
    return str;
}

[[nodiscard]] constexpr static auto capitalize(std::string str) -> std::string
{
    for (auto word : str | std::views::split(' ')) {
        if (!word.empty()) {
            auto first = word.begin();
            *first     = static_cast<char>(std::toupper(static_cast<unsigned char>(*first)));
        }
    }

    return str;
}

} // namespace Util
