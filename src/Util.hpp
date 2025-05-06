#pragma once

#include <ranges>
#include <print>
#include <charconv>

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

[[nodiscard]] static auto trim(std::string_view sv) -> std::string_view
{
    const auto not_space = [](char c) { return !std::isspace(static_cast<unsigned char>(c)); };
    sv.remove_prefix(std::ranges::distance(sv.begin(), std::ranges::find_if(sv, not_space)));
    sv.remove_suffix(std::ranges::distance(sv.rbegin(), std::ranges::find_if(sv | std::views::reverse, not_space)));

    return sv;
}


[[nodiscard]] static auto parse_number(std::string_view str) -> std::optional<std::size_t>
{
    std::size_t value = 0;
    auto [ptr, ec]    = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec != std::errc()) {
        std::println(stderr, "[ERROR] Failed to parse number from string: {}", str);
        return std::nullopt;
    }

    return value;
};

[[nodiscard]] static auto to_lower(std::string_view input) -> std::string
{
    std::string result;
    std::ranges::transform(input, std::back_inserter(result), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return result;
}

} // namespace Util
