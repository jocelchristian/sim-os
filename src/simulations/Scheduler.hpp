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
    using ProcessPtr   = std::shared_ptr<Os::Process>;
    using ProcessQueue = std::deque<ProcessPtr>;
    using ScheduleFn   = std::function<void(Scheduler&)>;

    ProcessPtr   running;
    ProcessQueue processes;
    ProcessQueue waiting;
    ProcessQueue ready;
    ScheduleFn   schedule_fn;
    std::size_t  timer = 0;

    template<std::invocable<Scheduler&> Callback>
    explicit Scheduler(Callback callback)
      : schedule_fn { callback }
    {}

    [[nodiscard]] auto complete() const -> bool;
    void               step();

    template<typename... Args>
    constexpr auto emplace_process(Args&&... args) -> ProcessPtr
    {
        return processes.emplace_back(std::make_shared<Os::Process>(std::forward<Args>(args)...));
    }

  private:
    void sidetrack_processes();
    void dispatch_process_by_first_event(const ProcessPtr& process);
    void update_waiting_list();
    void update_running();

    [[nodiscard]] auto ensure_pid_is_unique(const std::size_t pid) const -> bool;
};

} // namespace Simulations
