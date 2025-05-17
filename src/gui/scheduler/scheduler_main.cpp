#include <filesystem>
#include <print>

#include "lang/Interpreter.hpp"
#include "SchedulerApp.hpp"
#include "simulations/Scheduler.hpp"

auto main(int argc, const char** argv) -> int
{
    if (argc < 2) {
        std::println(stderr, "[ERROR] expected file path to simulation script");
        std::println("usage: scheduler <file.sl>");
    }

    const auto* const script_path          = argv[1];
    const auto        maybe_script_content = Util::read_entire_file(script_path);
    if (!maybe_script_content) { return 1; }

    using namespace Simulations;
    const auto schedule_policy = RoundRobinPolicy {};
    using SchedulePolicyType = decltype(schedule_policy);

    auto sim = std::make_shared<Scheduler<SchedulePolicyType>>(schedule_policy);

    if (!Interpreter::Interpreter<Scheduler<SchedulePolicyType>>::eval(*maybe_script_content, sim)) {
        std::println(stderr, "[ERROR] Could not correctly evaluate script {}", script_path);
    }

    auto app = SchedulerApp<SchedulePolicyType>::create(sim);
    if (!app) { return 1; }
    app->render();
}
