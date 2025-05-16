#pragma once

#include <limits>
#include <memory>

#include "os/Os.hpp"

namespace Simulations
{

// clang-format off
template<typename SimType>
concept Simulation = requires(SimType sim)
{
    { sim.complete } -> std::same_as<bool>;
    { sim.step };
};
// clang-format on

template<typename SchedulePolicy>
struct [[nodiscard]] Scheduler final
{
    using ProcessPtr   = std::shared_ptr<Os::Process>;
    using ProcessQueue = std::deque<ProcessPtr>;

    ProcessPtr     running;
    ProcessQueue   processes;
    ProcessQueue   waiting;
    ProcessQueue   ready;
    SchedulePolicy schedule_policy;
    std::size_t    timer     = 0;
    float          cpu_usage = 0;

    std::size_t max_processes             = std::numeric_limits<std::size_t>::max();
    std::size_t max_events_per_process    = std::numeric_limits<std::size_t>::max();
    std::size_t max_single_event_duration = std::numeric_limits<std::size_t>::max();
    std::size_t max_arrival_time          = std::numeric_limits<std::size_t>::max();

    double                  throughput              = 0;
    std::size_t             previous_finished_count = 0;
    std::vector<ProcessPtr> finished;

    template<std::invocable<Scheduler&> Policy>
    explicit Scheduler(Policy policy)
      : schedule_policy { policy }
    {}

    [[nodiscard]] auto complete() const -> bool
    {
        return !running && processes.empty() && waiting.empty() && ready.empty();
    }

    void step()
    {
        sidetrack_processes();
        update_waiting_list();
        update_running();

        if (!running) { schedule_policy(*this); }
        if (!running && ready.size() > 0) {
            running = ready.front();
            ready.pop_front();
        }

        if (running && !running->events.empty()) {
            const auto& next_event = running->events.front();
            cpu_usage              = next_event.resource_usage;
        }

        if (complete()) { cpu_usage = 0.0F; };

        throughput = timer != 0 ? (finished.size() - previous_finished_count) / static_cast<double>(timer) : 0.0;
        previous_finished_count = finished.size();

        ++timer;
    }

    template<typename... Args>
    constexpr auto emplace_process(Args&&... args) -> ProcessPtr
    {
        return processes.emplace_back(std::make_shared<Os::Process>(std::forward<Args>(args)...));
    }

    [[nodiscard]] auto average_waiting_time() const -> std::size_t
    {
        if (finished.empty()) { return 0; }

        std::size_t total_waiting_time = 0;
        for (const auto& process : finished) {
            if (!process->start_time.has_value()) { continue; }
            total_waiting_time += process->start_time.value() - process->arrival;
        }

        return total_waiting_time / finished.size();
    }

    [[nodiscard]] auto average_turnaround_time() const -> std::size_t
    {
        if (finished.empty()) { return 0; }

        std::size_t total_turnaround_time = 0;
        for (const auto& process : finished) {
            if (!process->finish_time.has_value()) { continue; }
            total_turnaround_time += process->finish_time.value() - process->arrival;
        }

        return total_turnaround_time / finished.size();
    }

  private:
    void sidetrack_processes()
    {
        for (auto it = processes.begin(); it != processes.end();) {
            auto process = *it;
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

    void dispatch_process_by_first_event(ProcessPtr& process)
    {

        static_assert(
          std::to_underlying(Os::EventKind::Count) == 2,
          "Exhaustive handling of all variants for enum EventKind is required."
        );
        assert(!process->events.empty() && "process queue must not be empty");
        const auto first_event = process->events.front();
        switch (first_event.kind) {
            case Os::EventKind::Cpu: {
                process->start_time = !process->start_time.has_value() ? std::optional { timer } : std::nullopt;
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

    void update_waiting_list()
    {
        for (auto it = waiting.begin(); it != waiting.end();) {
            auto& process = *it;
            assert(!process->events.empty() && "event queue must not be empty");

            auto& current_event = process->events.front();
            assert(current_event.kind == Os::EventKind::Io && "process in waiting queue must be on an IO event");
            assert(current_event.duration > 0);
            --current_event.duration;

            if (current_event.duration == 0) {
                process->events.pop_front();
                if (!process->events.empty()) {
                    dispatch_process_by_first_event(process);
                } else {
                    process->finish_time = !process->finish_time.has_value() ? std::optional { timer } : std::nullopt;
                    finished.push_back(process);
                }

                it = waiting.erase(it);
            } else {
                ++it;
            }
        }
    }

    void update_running()
    {
        if (!running) { return; }

        auto& process = running;
        assert(!process->events.empty() && "event queue must not be empty");

        auto& current_event = process->events.front();
        assert(current_event.kind == Os::EventKind::Cpu && "process running must be on an CPU event");
        assert(current_event.duration > 0);
        --current_event.duration;

        if (current_event.duration == 0) {
            process->events.pop_front();
            if (!process->events.empty()) {
                dispatch_process_by_first_event(process);
            } else {
                finished.push_back(process);
            }

            running = nullptr;
        }
    }

    [[nodiscard]] auto ensure_pid_is_unique(const std::size_t pid) const -> bool
    {
        const auto comparator = [&](const auto& elem) { return elem->pid == pid; };
        return (!running || running->pid != pid) && std::ranges::find_if(ready, comparator) == ready.end()
               && std::ranges::find_if(waiting, comparator) == waiting.end();
    }
};

struct [[nodiscard]] RoundRobinPolicy
{
    constexpr static auto POLICY_NAME = "Round Robin";

    template<typename T>
    void operator()(Scheduler<T>& sim) const
    {
        if (sim.ready.empty()) { return; }

        const auto process = sim.ready.front();
        sim.ready.pop_front();
        sim.running = process;

        auto& events = process->events;
        assert(!events.empty() && "process queue must not be empty");
        auto& next_event = events.front();
        assert(next_event.kind == Os::EventKind::Cpu && "event of process in ready must be cpu");

        if (next_event.duration > quantum) {
            next_event.duration -= quantum;
            const auto new_event = Os::Event {
                .kind     = Os::EventKind::Cpu,
                .duration = quantum,
            };
            events.push_front(new_event);
        }
    }

    std::size_t quantum = 5;
};

} // namespace Simulations
