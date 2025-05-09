#include "Scheduler.hpp"

namespace Simulations
{

[[nodiscard]] auto Scheduler::complete() const -> bool
{
    return !running && processes.empty() && waiting.empty() && ready.empty();
}

void Scheduler::step()
{
    sidetrack_processes();
    update_waiting_list();
    update_running();

    if ((schedule_fn != nullptr) && !running) { schedule_fn(*this); }
    if (!running && ready.size() > 0) {
        running = ready.front();
        ready.pop_front();
    }

    ++timer;
}

void Scheduler::sidetrack_processes()
{
    for (auto it = processes.begin(); it != processes.end();) {
        const auto process = *it;
        if (process->arrival != timer) {
            ++it;
            continue;
        }

        if (!ensure_pid_is_unique(process->pid)) {
            std::println(
              stderr, "[ERROR] process {} with pid {} is already in use, skipping...", process->name, process->pid
            );
            continue;
        }

        if (process->events.empty()) {
            std::println(
              stderr,
              "[ERROR] process {} with pid {} should at least have one event, skipping...",
              process->name,
              process->pid
            );
            continue;
        }

        dispatch_process_by_first_event(process);
        it = processes.erase(it);
    }
}

void Scheduler::dispatch_process_by_first_event(const ProcessPtr& process)
{
    static_assert(
      std::to_underlying(Os::EventKind::Count) == 2,
      "Exhaustive handling of all variants for enum EventKind is required."
    );
    assert(!process->events.empty() && "process queue must not be empty");
    const auto first_event = process->events.front();
    switch (first_event.kind) {
        case Os::EventKind::Cpu: {
            ready.push_back(process);
            break;
        }
        case Os::EventKind::Io: {
            waiting.push_back(process);
            break;
        }
        default: {
            assert(false && "unreachable");
        }
    }
}

void Scheduler::update_waiting_list()
{
    for (auto it = waiting.begin(); it != waiting.end();) {
        auto& process = *it;
        assert(!process->events.empty() && "event queue must not be empty");

        auto& current_event = process->events.front();
        assert(current_event.kind == Os::EventKind::Io && "process in waiting queue must be on an IO event");
        --current_event.duration;

        if (current_event.duration == 0) {
            process->events.pop_front();
            if (!process->events.empty()) { dispatch_process_by_first_event(process); }

            it = waiting.erase(it);
        } else {
            ++it;
        }
    }
}

void Scheduler::update_running()
{
    if (!running) { return; }

    auto& process = running;
    assert(!process->events.empty() && "event queue must not be empty");

    auto& current_event = process->events.front();
    assert(current_event.kind == Os::EventKind::Cpu && "process running must be on an CPU event");
    --current_event.duration;

    if (current_event.duration == 0) {
        process->events.pop_front();
        if (!process->events.empty()) {
            dispatch_process_by_first_event(process);
        } else {
            std::println("Process {} with pid {} has terminated", process->name, process->pid);
        }

        running = nullptr;
    }
}


[[nodiscard]] auto Scheduler::ensure_pid_is_unique(const std::size_t pid) const -> bool
{
    const auto comparator = [&](const auto& elem) { return elem->pid == pid; };
    return (!running || running->pid != pid) && std::ranges::find_if(ready, comparator) == ready.end()
           && std::ranges::find_if(waiting, comparator) == waiting.end();
}

} // namespace Simulations
