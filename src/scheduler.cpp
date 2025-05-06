#include <algorithm>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

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

enum class EventKind : std::uint8_t
{
    Cpu = 0,
    Io,
    Count,
};

[[nodiscard]] constexpr static auto event_kind_to_str(EventKind event_kind) -> std::string
{
    static_assert(
      std::to_underlying(EventKind::Count) == 2,
      "[ERROR] Exhaustive handling of all enum variants for EventKind is required"
    );

    switch (event_kind) {
        case EventKind::Cpu: {
            return "cpu";
        }
        case EventKind::Io: {
            return "io";
        }
        default: {
            assert(false && "unreachable");
        }
    }
}

[[nodiscard]] constexpr static auto event_kind_try_from_str(std::string_view str) -> std::optional<EventKind>
{
    static_assert(
      std::to_underlying(EventKind::Count) == 2,
      "[ERROR] Exhaustive handling of all enum variants for EventKind is required"
    );

    if (Util::to_lower(str) == "cpu") {
        return EventKind::Cpu;
    } else if (Util::to_lower(str) == "io") {
        return EventKind::Io;
    }

    std::println("[ERROR] Unknown event kind: {}", str);
    return std::nullopt;
}

struct [[nodiscard]] Event final
{
    EventKind   kind;
    std::size_t duration;
};

struct [[nodiscard]] Process final
{
    std::string       name;
    std::size_t       pid;
    std::size_t       arrival;
    std::deque<Event> events;

    [[nodiscard]] static auto load_from_file(const std::filesystem::path &path) -> std::optional<Process>
    {
        auto file = std::ifstream(path);
        if (!file) { std::println(stderr, "[ERROR] Unable to read file {}: {}", path.string(), strerror(errno)); }

        std::string       name;
        std::size_t       pid     = 0;
        std::size_t       arrival = 0;
        std::deque<Event> events;

        bool        parsing_header = true;
        std::string line;
        while (std::getline(file, line)) {
            // clang-format off
            auto it = line
                | std::views::split(',')
                | std::views::transform([](auto &&elem) {
                    return Util::trim(std::string_view{ elem });
                });
            // clang-format on

            if (parsing_header) {
                const std::vector<std::string_view> header(it.begin(), it.end());
                if (header.size() != 3) {
                    std::println(
                      stderr,
                      "[ERROR] expected (name, pid, arrival) in process file "
                      "format header but got: {}",
                      line
                    );
                    return std::nullopt;
                }

                name    = header[0];
                pid     = TRY(Util::parse_number(header[1]));
                arrival = TRY(Util::parse_number(header[2]));

                parsing_header = false;
                continue;
            }

            const std::vector<std::string_view> event(it.begin(), it.end());
            if (event.size() != 2) {
                std::println(
                  stderr,
                  "[ERROR] expected (kind, duration) in process file"
                  " event description but got: {}",
                  line
                );
                return std::nullopt;
            }


            std::size_t duration   = TRY(Util::parse_number(event[1]));
            const auto  event_kind = TRY(event_kind_try_from_str(event[0]));
            events.emplace_back(event_kind, duration);
        }

        return Process{ .pid = pid, .arrival = arrival, .events = events };
    }
};

struct Scheduler;

class [[nodiscard]] Simulation final
{
  public:
    using ScheduleFn = std::function<void(Simulation &)>;

  public:
    template<std::invocable<Simulation &> Callback>
    explicit Simulation(Callback callback)
      : schedule_fn{ callback }
    {}

    [[nodiscard]] auto complete() const -> bool
    {
        return !running && processes.empty() && waiting.empty() && ready.empty();
    }

    void add_process(const Process &process) { processes.push_back(std::make_shared<Process>(process)); }

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
    [[nodiscard]] auto ready_queue() -> std::deque<std::shared_ptr<Process>> & { return ready; }

    void set_running(const std::shared_ptr<Process> &process) { running = process; }

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

    void dispatch_process_by_first_event(const std::shared_ptr<Process> &process)
    {
        static_assert(
          std::to_underlying(EventKind::Count) == 2,
          "Exhaustive handling of all variants for enum EventKind is required."
        );
        assert(!process->events.empty() && "process queue must not be empty");
        const auto first_event = process->events.front();
        switch (first_event.kind) {
            case EventKind::Cpu: {
                ready.push_back(process);
                break;
            }
            case EventKind::Io: {
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
            auto &process = *it;
            assert(!process->events.empty() && "event queue must not be empty");

            auto &current_event = process->events.front();
            assert(current_event.kind == EventKind::Io && "process in waiting queue must be on an IO event");
            --current_event.duration;

            if (current_event.duration == 0) {
                process->events.pop_front();
                it = waiting.erase(it);

                if (!process->events.empty()) {
                    dispatch_process_by_first_event(process);
                }
            } else {
                ++it;
            }
        }
    }

    void update_running()
    {
        if (!running) { return; }

        auto &process = running;
        assert(!process->events.empty() && "event queue must not be empty");

        auto &current_event = process->events.front();
        assert(current_event.kind == EventKind::Cpu && "process running must be on an CPU event");
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
        const auto comparator = [&](const auto &elem) { return elem->pid == pid; };
        return (!running || running->pid != pid) && std::ranges::find_if(ready, comparator) == ready.end()
               && std::ranges::find_if(waiting, comparator) == waiting.end();
    }

    static void print_process_deque(const std::deque<std::shared_ptr<Process>> &processes)
    {
        // FIXME: implement a custom formatter for Process
        for (const auto &process : processes) {
            std::println(
              "\n    {{ name: {}, pid: {}, arrival: {}, events: {{ ", process->name, process->pid, process->arrival
            );
            for (const auto &event : process->events) {
                std::println("        {{ kind: {}, duration: {} }},\n    }},", event_kind_to_str(event.kind), event.duration);
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
            std::print(
              "{{ name: {}, pid: {}, arrival: {}, events: {{ ", running->name, running->pid, running->arrival
            );
            for (const auto &event : running->events) {
                std::println("kind: {}, duration: {} }},", event_kind_to_str(event.kind), event.duration);
            }
        }
        std::println("}}");
    }

  private:
    std::shared_ptr<Process>             running;
    std::deque<std::shared_ptr<Process>> processes;
    std::deque<std::shared_ptr<Process>> waiting;
    std::deque<std::shared_ptr<Process>> ready;

    ScheduleFn schedule_fn;

    std::size_t timer = 0;
};

auto main() -> int
{
    const auto round_robin_scheduler = [quantum = 5UL](auto &sim) {
        if (sim.ready_queue().empty()) { return; }

        auto       &ready   = sim.ready_queue();
        const auto &process = ready.front();
        ready.pop_front();
        sim.set_running(process);

        auto &events = process->events;
        assert(!events.empty() && "process queue must not be empty");
        auto &next_event = events.front();
        assert(next_event.kind == EventKind::Cpu && "event of process in ready must be cpu");

        // Split event in multiple events if greater than quantum
        if (next_event.duration > quantum) {
            next_event.duration -= quantum;
            const auto new_event = Event{
                .kind     = EventKind::Cpu,
                .duration = quantum,
            };
            events.push_front(new_event);
        }
    };

    auto sim = Simulation(round_robin_scheduler);

    const auto process = Process::load_from_file("test.csv");
    if (!process) { return 1; }
    sim.add_process(*process);

    while (!sim.complete()) { sim.step(); }
}
