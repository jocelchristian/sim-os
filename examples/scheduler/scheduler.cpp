#include <cassert>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <print>

#include "lang/Interpreter.hpp"
#include "os/Os.hpp"
#include "simulations/Scheduler.hpp"

auto main() -> int
{
    const auto round_robin_scheduler = [quantum = 5UL](auto& sim) {
        if (sim.ready.empty()) { return; }

        const auto process = sim.ready.front();
        sim.ready.pop_front();
        sim.running = process;

        auto& events = process->events;
        assert(!events.empty() && "process queue must not be empty");
        auto& next_event = events.front();
        assert(next_event.kind == Os::EventKind::Cpu && "event of process in ready must be cpu");

        // Split event in multiple events if greater than quantum
        if (next_event.duration > quantum) {
            next_event.duration -= quantum;
            const auto new_event = Os::Event {
                .kind     = Os::EventKind::Cpu,
                .duration = quantum,
            };
            events.push_front(new_event);
        }
    };

    auto sim = Simulations::Scheduler(round_robin_scheduler);


    constexpr std::string_view script = "examples/scheduler/simple.sl";
    const auto                 path   = std::filesystem::path(script);
    auto                       file   = std::ifstream(path);
    if (!file) { std::println(stderr, "[ERROR] Unable to read file {}: {}", path.string(), strerror(errno)); }

    std::stringstream ss;
    ss << file.rdbuf();
    const auto file_content = ss.str();

    if (!Interpreter::Interpreter<decltype(sim)>::eval(file_content, sim)) {
        std::println(stderr, "[ERROR] Could not correctly evaluate script {}", script);
    }

    while (!sim.complete()) { sim.step(); }
}
