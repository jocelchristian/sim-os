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
    auto sim = std::make_shared<Scheduler>(named_scheduler_from_policy(SchedulePolicy::RoundRobin));
    if (!Interpreter::Interpreter<Scheduler>::eval(*maybe_script_content, sim)) {
        std::println(stderr, "[ERROR] Could not correctly evaluate script {}", script_path);
    }

    auto app = SchedulerApp::create(sim);
    if (!app) { return 1; }
    app->render();
}
