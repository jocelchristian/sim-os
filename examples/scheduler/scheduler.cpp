#include <cassert>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <print>
#include <utility>

#include "lang/Interpreter.hpp"
#include "os/Os.hpp"
#include "os/Process.hpp"


struct Scheduler;

class [[nodiscard]] SimOs final
{
  public:
    using ScheduleFn = std::function<void(SimOs&)>;

  public:
    template<std::invocable<SimOs&> Callback>
    explicit SimOs(Callback callback)
      : schedule_fn { callback }
    {}

    [[nodiscard]] auto complete() const -> bool
    {
        return !running && processes.empty() && waiting.empty() && ready.empty();
    }

    template<typename... Args>
    void emplace_process(Args&&... args)
    {
        processes.push_back(std::make_shared<Os::Process>(std::forward<Args>(args)...));
    }

    void step()
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

    // FIXME: deducing this
    [[nodiscard]] auto ready_queue() -> std::deque<std::shared_ptr<Os::Process>>& { return ready; }

    void set_running(const std::shared_ptr<Os::Process>& process) { running = process; }

  private:
    void sidetrack_processes()
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

    void dispatch_process_by_first_event(const std::shared_ptr<Os::Process>& process)
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

    void update_waiting_list()
    {
        for (auto it = waiting.begin(); it != waiting.end();) {
            auto& process = *it;
            assert(!process->events.empty() && "event queue must not be empty");

            auto& current_event = process->events.front();
            assert(current_event.kind == Os::EventKind::Io && "process in waiting queue must be on an IO event");
            --current_event.duration;

            if (current_event.duration == 0) {
                process->events.pop_front();
                it = waiting.erase(it);

                if (!process->events.empty()) { dispatch_process_by_first_event(process); }
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

    [[nodiscard]] auto ensure_pid_is_unique(const std::size_t pid) const -> bool
    {
        const auto comparator = [&](const auto& elem) { return elem->pid == pid; };
        return (!running || running->pid != pid) && std::ranges::find_if(ready, comparator) == ready.end()
               && std::ranges::find_if(waiting, comparator) == waiting.end();
    }

    static void print_process_deque(const std::deque<std::shared_ptr<Os::Process>>& processes)
    {
        // FIXME: implement a custom formatter for Process
        for (const auto& process : processes) {
            std::println(
              "\n    {{ name: {}, pid: {}, arrival: {}, events: {{ ", process->name, process->pid, process->arrival
            );
            for (const auto& event : process->events) {
                std::println(
                  "        {{ kind: {}, duration: {} }},\n    }},", event_kind_to_str(event.kind), event.duration
                );
            }
        }
    }

    void print_all_queues()
    {
        std::print("Ready Queue {{");
        print_process_deque(ready);
        std::println("}}");

        std::println("Waiting Queue {{");
        print_process_deque(waiting);
        std::println("}}");

        std::print("Running Process {{");
        if (running) {
            std::print("{{ name: {}, pid: {}, arrival: {}, events: {{ ", running->name, running->pid, running->arrival);
            for (const auto& event : running->events) {
                std::println("kind: {}, duration: {} }},", event_kind_to_str(event.kind), event.duration);
            }
        }
        std::println("}}");
    }

  private:
    std::shared_ptr<Os::Process>             running;
    std::deque<std::shared_ptr<Os::Process>> processes;
    std::deque<std::shared_ptr<Os::Process>> waiting;
    std::deque<std::shared_ptr<Os::Process>> ready;

    ScheduleFn schedule_fn;

    std::size_t timer = 0;
};

auto main() -> int
{
    const auto round_robin_scheduler = [quantum = 5UL](auto& sim) {
        if (sim.ready_queue().empty()) { return; }

        auto&       ready   = sim.ready_queue();
        const auto& process = ready.front();
        ready.pop_front();
        sim.set_running(process);

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

    auto sim = SimOs(round_robin_scheduler);


    constexpr std::string_view script = "examples/scheduler/simple.sl";
    const auto                 path   = std::filesystem::path(script);
    auto                       file   = std::ifstream(path);
    if (!file) { std::println(stderr, "[ERROR] Unable to read file {}: {}", path.string(), strerror(errno)); }

    std::stringstream ss;
    ss << file.rdbuf();
    const auto file_content = ss.str();

    if (!Interpreter::Interpreter<SimOs>::eval(file_content, sim)) {
        std::println(stderr, "[ERROR] Could not correctly evaluate script {}", script);
    }

    while (!sim.complete()) { sim.step(); }
}
