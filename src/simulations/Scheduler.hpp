#pragma once

#include <functional>
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

struct [[nodiscard]] Scheduler final
{
    using ScheduleFn = std::function<void(Scheduler&)>;

    std::shared_ptr<Os::Process>             running;
    std::deque<std::shared_ptr<Os::Process>> processes;
    std::deque<std::shared_ptr<Os::Process>> waiting;
    std::deque<std::shared_ptr<Os::Process>> ready;
    ScheduleFn                               schedule_fn;
    std::size_t                              timer = 0;

    template<std::invocable<Scheduler&> Callback>
    explicit Scheduler(Callback callback)
      : schedule_fn { callback }
    {}

    [[nodiscard]] auto complete() const -> bool;
    void               step();

    template <typename... Args>
    constexpr auto emplace_process(Args&&... args) -> std::shared_ptr<Os::Process>
    {
        return processes.emplace_back(std::make_shared<Os::Process>(std::forward<Args>(args)...));
    }

private:
    void sidetrack_processes();
    void dispatch_process_by_first_event(const std::shared_ptr<Os::Process>& process);
    void update_waiting_list();
    void update_running();

    [[nodiscard]] auto ensure_pid_is_unique(const std::size_t pid) const -> bool;

    static void print_process_deque(const std::string_view name, const std::deque<std::shared_ptr<Os::Process>>& processes);
    void print_all_queues() const;
};

} // namespace Simulations
