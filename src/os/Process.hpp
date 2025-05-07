#pragma once

#include "Os.hpp"
#include <deque>
#include <string_view>

namespace Os
{

struct [[nodiscard]] Process final
{
    std::string_view      name;
    std::size_t           pid;
    std::size_t           arrival;
    std::deque<Event> events;
};

} // namespace Os
