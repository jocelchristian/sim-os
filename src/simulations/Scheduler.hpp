#pragma once

#include <cstddef>
#include <limits>
#include <memory>

#include "os/Os.hpp"

namespace Simulations
{

template<typename SchedulePolicy>
struct [[nodiscard]] Scheduler final
{
    constexpr static auto MAX_THREADS = 9;

    using ProcessPtr   = std::shared_ptr<Os::Process>;
    using ProcessQueue = std::deque<ProcessPtr>;

    std::array<ProcessPtr, MAX_THREADS>   running;
    std::array<ProcessQueue, MAX_THREADS> processes;
    std::array<ProcessQueue, MAX_THREADS> waiting;
    std::array<ProcessQueue, MAX_THREADS> ready;

    SchedulePolicy                 schedule_policy;
    std::size_t                    timer     = 0;
    std::array<float, MAX_THREADS> cpu_usage = {};

    std::size_t max_processes             = std::numeric_limits<std::size_t>::max();
    std::size_t max_events_per_process    = std::numeric_limits<std::size_t>::max();
    std::size_t max_single_event_duration = std::numeric_limits<std::size_t>::max();
    std::size_t max_arrival_time          = std::numeric_limits<std::size_t>::max();
    std::size_t threads_count             = MAX_THREADS;

    std::size_t next_thread = 0;

    double                  throughput              = 0;
    std::size_t             previous_finished_count = 0;
    std::vector<ProcessPtr> finished;

    template<std::invocable<Scheduler&> Policy>
    explicit Scheduler(Policy policy)
      : schedule_policy { policy }
    {}

    ~Scheduler() = default;

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Scheduler(Scheduler&&) noexcept = default;
    Scheduler& operator=(Scheduler&&) noexcept = default;

    [[nodiscard]] auto complete() const -> bool
    {
        const auto any_running   = std::ranges::any_of(running, [](const auto& process) { return process != nullptr; });
        const auto any_processes = std::ranges::any_of(processes, [](const auto& elem) { return !elem.empty(); });
        const auto any_ready     = std::ranges::any_of(ready, [](const auto& elem) { return !elem.empty(); });
        const auto any_waiting   = std::ranges::any_of(waiting, [](const auto& elem) { return !elem.empty(); });

        return !any_running && !any_processes && !any_ready && !any_waiting;
    }

    void step()
    {
        for (std::size_t thread_idx = 0; thread_idx < threads_count; ++thread_idx) {
            sidetrack_processes(thread_idx);
            update_waiting_list(thread_idx);
            update_running(thread_idx);

            if (!running[thread_idx]) { schedule_policy(*this); }
            if (!running[thread_idx] && ready[thread_idx].size() > 0) {
                running[thread_idx] = ready[thread_idx].front();
                ready[thread_idx].pop_front();
            }

            if (running[thread_idx] && !running[thread_idx]->events.empty()) {
                const auto& next_event = running[thread_idx]->events.front();
                cpu_usage[thread_idx]  = next_event.resource_usage;
            }

            if (complete()) { cpu_usage.fill(0.0F); };

            throughput              = timer != 0 ? static_cast<double>(finished.size()) / static_cast<double>(timer) : 0.0;
            previous_finished_count = finished.size();
        }

        ++timer;
    }

    template<typename... Args>
    constexpr auto emplace_process(Args&&... args) -> ProcessPtr
    {
        const auto ret =
          processes[next_thread].emplace_back(std::make_shared<Os::Process>(std::forward<Args>(args)...));
        next_thread = (next_thread + 1) % threads_count;
        return ret;
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

    [[nodiscard]] auto average_cpu_usage() const -> double
    {
        double total_usage = 0;
        for (std::size_t thread_idx = 0; thread_idx < threads_count; ++thread_idx) {
            total_usage += cpu_usage[thread_idx];
        }

        return total_usage / static_cast<double>(threads_count);
    }

  private:
    void sidetrack_processes(const std::size_t thread_idx)
    {
        auto& procs = processes[thread_idx];
        for (auto it = procs.begin(); it != procs.end();) {
            auto process = *it;
            if (process->arrival != timer) {
                ++it;
                continue;
            }

            if (!ensure_pid_is_unique(thread_idx, process->pid)) {
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

            dispatch_process_by_first_event(thread_idx, process);
            it = procs.erase(it);
        }
    }

    void dispatch_process_by_first_event(const std::size_t thread_idx, ProcessPtr& process)
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
                ready[thread_idx].push_back(process);
                break;
            }
            case Os::EventKind::Io: {
                waiting[thread_idx].push_back(process);
                break;
            }
            default: {
                assert(false && "unreachable");
            }
        }
    }

    void update_waiting_list(const std::size_t thread_idx)
    {
        auto& waits = waiting[thread_idx];
        std::vector<ProcessPtr> to_dispatch;

        for (auto it = waits.begin(); it != waits.end();) {
            auto& process = *it;
            assert(!process->events.empty() && "event queue must not be empty");

            auto& current_event = process->events.front();
            assert(current_event.kind == Os::EventKind::Io && "process in waits queue must be on an IO event");
            assert(current_event.duration > 0);
            --current_event.duration;

            if (current_event.duration == 0) {
                process->events.pop_front();
                if (!process->events.empty()) {
                    to_dispatch.push_back(process);
                } else {
                    process->finish_time = !process->finish_time.has_value() ? std::optional { timer } : std::nullopt;
                    finished.push_back(process);
                }

                it = waits.erase(it);
            } else {
                ++it;
            }
        }

        for (auto& process: to_dispatch) {
            dispatch_process_by_first_event(thread_idx, process);
        }
    }

    void update_running(const std::size_t thread_idx)
    {
        if (!running[thread_idx]) { return; }

        auto& process = running[thread_idx];
        assert(!process->events.empty() && "event queue must not be empty");

        auto& current_event = process->events.front();
        assert(current_event.kind == Os::EventKind::Cpu && "process running must be on an CPU event");
        assert(current_event.duration > 0);
        --current_event.duration;

        if (current_event.duration == 0) {
            process->events.pop_front();
            if (!process->events.empty()) {
                dispatch_process_by_first_event(thread_idx, process);
            } else {
                finished.push_back(process);
            }

            running[thread_idx] = nullptr;
        }
    }

    [[nodiscard]] auto ensure_pid_is_unique(const std::size_t thread_idx, const std::size_t pid) const -> bool
    {
        const auto comparator = [&](const auto& elem) { return elem->pid == pid; };
        return (!running[thread_idx] || running[thread_idx]->pid != pid)
               && std::ranges::find_if(ready[thread_idx], comparator) == ready[thread_idx].end()
               && std::ranges::find_if(waiting[thread_idx], comparator) == waiting[thread_idx].end();
    }
};

struct [[nodiscard]] RoundRobinPolicy
{
    constexpr static auto POLICY_NAME = "Round Robin";

    template<typename T>
    void operator()(Scheduler<T>& sim) const
    {
        for (std::size_t thread_idx = 0; thread_idx < sim.threads_count; ++thread_idx) {
            auto& ready = sim.ready[thread_idx];

            if (ready.empty()) { return; }

            const auto process = ready.front();
            ready.pop_front();
            sim.running[thread_idx] = process;

            auto& events = process->events;
            assert(!events.empty() && "process queue must not be empty");
            auto& next_event = events.front();
            assert(next_event.kind == Os::EventKind::Cpu && "event of process in ready must be cpu");

            if (next_event.duration > quantum) {
                next_event.duration -= quantum;
                const auto new_event = Os::Event {
                    .kind     = Os::EventKind::Cpu,
                    .duration = quantum,
                    .resource_usage = next_event.resource_usage,
                };
                events.push_front(new_event);
            }
        }
    }

    std::size_t quantum = 5;
};

} // namespace Simulations
