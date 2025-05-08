#include "Scheduler.hpp"

namespace Simulations
{

[[nodiscard]] auto Scheduler::complete() const -> bool
{
    return !running && processes.empty() && waiting.empty() && ready.empty();
}

void Scheduler::step()
{
    std::println("--- Stepping simulation (timer: {}) ---", timer);

    std::println("1. Switch all available processes in either ready/waiting queue");
    // 1. Switch all available processes in either the ready queue or the
    // waiting queue
    sidetrack_processes();

    print_all_queues();

    std::println("2. Scan waiting list and dispatch all processes whose wait terminates");
    // 2. Scan waiting list and put in ready queue all the processes whose
    // event terminates in this timer
    update_waiting_list();

    print_all_queues();

    std::println("3. Update the running process");
    // 3. Decrement event of running & eventually reschedule if event is over
    update_running();

    print_all_queues();

    std::println("4. Call the scheduler");
    // 4. Call the scheduler
    if ((schedule_fn != nullptr) && !running) { schedule_fn(*this); }
    if (!running && ready.size() > 0) {
        running = ready.front();
        ready.pop_front();
    }

    print_all_queues();

    // 5. Update timer
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

void Scheduler::dispatch_process_by_first_event(const std::shared_ptr<Os::Process>& process)
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

void Scheduler::print_process_deque(const std::string_view name, const std::deque<std::shared_ptr<Os::Process>>& processes)
{
    std::println("{} [", name);
    for (const auto& process : processes) {
        if (process) {
            std::println("    {}", *process);
        }
    }
    std::println("]");
}

void Scheduler::print_all_queues() const
{
    print_process_deque("Ready", ready);
    print_process_deque("Waiting", waiting);
    if (running) { std::println("Running = {:s}", *running); }
}


} // namespace Simulations
