#pragma once

#include <memory>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <stb_image.h>

#include "gui/Gui.hpp"
#include "simulations/Scheduler.hpp"

template<typename SchedulePolicy>
class [[nodiscard]] SchedulerApp final
{
  public:
    constexpr static auto WINDOW_WIDTH     = 1920;
    constexpr static auto WINDOW_HEIGHT    = 1080;
    constexpr static auto BACKGROUND_COLOR = ImVec4(0.94, 0.94, 0.94, 1.0);
    constexpr static auto BUTTON_SIZE      = ImVec2(16, 16);

  public:
    [[nodiscard]] static auto create(const auto& sim)
      -> std::unique_ptr<SchedulerApp>
    {
        const auto window = Gui::init_window("sim-os: scheduler", WINDOW_WIDTH, WINDOW_HEIGHT);
        if (!window) { return nullptr; }
        return std::unique_ptr<SchedulerApp>(new SchedulerApp { *window, sim });
    }

    void render()
    {
        ImVec4 clear_color = BACKGROUND_COLOR;

        maybe_previous_texture_id = Gui::load_texture("resources/previous.png");
        maybe_next_texture_id     = Gui::load_texture("resources/next.png");

        while (!quit) {
            stepped_this_frame = false;
            if (glfwWindowShouldClose(window) == 1) { quit = true; }

            if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                if (!sim->complete() && !stepped_this_frame) {
                    sim->step();
                    stepped_this_frame = true;
                }
            }

            glfwPollEvents();
            if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) { continue; }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Main window
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0);
            ImGui::SetNextWindowPos(ImVec2(0.0, 0.0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("sim-os: scheduler", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
            {
                draw_control_buttons();

                const auto spacing                = ImGui::GetStyle().ItemSpacing.x;
                const auto available_screen_space = ImGui::GetContentRegionAvail();
                const auto group_width            = (available_screen_space.x - spacing * 2) / 3.0F;
                const auto group_height           = available_screen_space.y - 30.0F;

                ImGui::BeginGroup();
                draw_process_queue("Ready", sim->ready, ImVec2(group_width, group_height));
                ImGui::EndGroup();

                ImGui::SameLine();

                ImGui::BeginGroup();
                draw_process_queue("Waiting", sim->waiting, ImVec2(group_width, (group_height / 2.0F) - 16.0F));
                draw_process_queue("Arrival", sim->processes, ImVec2(group_width, (group_height / 2.0F) - 16.0F));
                ImGui::EndGroup();

                ImGui::SameLine();

                ImGui::BeginGroup();
                draw_running_process(ImVec2(group_width, (group_height / 2.0F) - 16.0F));
                draw_statistics(ImVec2(group_width, (group_height / 2.0F) - 16.0F));
                ImGui::EndGroup();
            }
            ImGui::End();
            ImGui::PopStyleVar(1);

            ImGui::Render();
            int display_w = 0;
            int display_h = 0;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(
              clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w
            );
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    }

    void draw_statistics(const ImVec2& child_size)
    {
        const auto draw_table_element = [](const auto& key, const auto& value) -> void {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            Gui::draw_text_formatted("{}", key);

            ImGui::TableSetColumnIndex(1);
            Gui::draw_text_formatted("{}", value);
        };

        Gui::draw_titled_child("Stats", child_size, [&] -> void {
            if (ImGui::BeginChild("Simulation Statistics", child_size, 1, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                if (ImGui::BeginTable("Stats Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

                    draw_table_element("Scheduler policy", SchedulePolicy::POLICY_NAME);
                    draw_table_element("Ready queue size", sim->ready.size());
                    draw_table_element("Waiting queue size", sim->waiting.size());
                    draw_table_element("Arrival size", sim->processes.size());
                    draw_table_element("Timer", sim->timer);

                    ImGui::EndTable();
                }
            }

            ImGui::EndChild();
        });
    }

    void draw_control_buttons()
    {
        // TODO: implement previous
        const auto spacing         = ImGui::GetStyle().ItemSpacing.x;
        const auto total_width     = (BUTTON_SIZE.x * 2.0F) + spacing;
        const auto available_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((available_width - total_width) * 0.5F);

        // Fallback
        if (!maybe_previous_texture_id || !maybe_next_texture_id) {
            if (ImGui::Button("Previous")) {}

            ImGui::SameLine();

            if (ImGui::Button("Next") && !sim->complete()) { sim->step(); }

            return;
        }

        auto* const previous_texture_id =
          reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_previous_texture_id));
        if (ImGui::ImageButton(previous_texture_id, BUTTON_SIZE)) {}

        ImGui::SameLine();

        auto* const next_texture_id = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_next_texture_id));
        if (ImGui::ImageButton(next_texture_id, BUTTON_SIZE) && !sim->complete()) { sim->step(); }
    }

    static void draw_process(const auto& process)
    {
        if (process == nullptr) { return; }

        if (ImGui::BeginChild("Scrollable Process", ImVec2(0, 0), 1, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            if (ImGui::CollapsingHeader(std::string { process->name }.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();

                Gui::draw_text_formatted("Pid: {}", process->pid);
                Gui::draw_text_formatted("Arrival Time: {}", process->arrival);

                draw_events_table(process->events);

                ImGui::Unindent();
            }

            ImGui::EndChild();
        }
    }

    void draw_running_process(const ImVec2& child_size)
    {
        Gui::draw_titled_child("Running", child_size, [&] -> void {
            if (ImGui::BeginChild("Scrollable Process", child_size, 1, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                if (sim->running != nullptr) {
                    if (ImGui::CollapsingHeader(
                          std::string { sim->running->name }.c_str(), ImGuiTreeNodeFlags_DefaultOpen
                        )) {
                        ImGui::Indent();

                        Gui::draw_text_formatted("Pid: {}", sim->running->pid);
                        Gui::draw_text_formatted("Arrival Time: {}", sim->running->arrival);

                        draw_events_table(sim->running->events);

                        ImGui::Unindent();
                    }
                }
            }
            ImGui::EndChild();
        });
    }

    static void draw_process_queue(
      const std::string_view                                               title,
      const auto& processes,
      const ImVec2&                                                        child_size
    )
    {
        const auto null_terminated = std::string { title };

        Gui::draw_titled_child(null_terminated, child_size, [&] -> void {
            if (ImGui::BeginChild(null_terminated.c_str(), child_size, 1, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                for (const auto& process : processes) { draw_process(process); }
            }

            ImGui::EndChild();
        });
    }

    static void draw_events_table(const Os::Process::EventsQueue& events)
    {
        if (!events.empty()) {
            if (ImGui::BeginTable("EventsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Event");
                ImGui::TableSetupColumn("Duration");
                ImGui::TableHeadersRow();

                for (const auto& event : events) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    Gui::draw_text_formatted("{}", event.kind);

                    ImGui::TableSetColumnIndex(1);
                    Gui::draw_text_formatted("{}", event.duration);
                }

                ImGui::EndTable();
            }
        }
    }

    ~SchedulerApp()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    SchedulerApp(const SchedulerApp&)            = delete;
    SchedulerApp& operator=(const SchedulerApp&) = delete;
    SchedulerApp(SchedulerApp&&)                 = delete;
    SchedulerApp& operator=(SchedulerApp&&)      = delete;

  private:
    explicit SchedulerApp(GLFWwindow* window, const std::shared_ptr<Simulations::Scheduler<SchedulePolicy>>& sim)
      : window { window },
        sim { sim }
    {}

  private:
    GLFWwindow*                                             window = nullptr;
    bool                                                    quit   = false;
    std::shared_ptr<Simulations::Scheduler<SchedulePolicy>> sim;
    bool                                                    stepped_this_frame = false;
    std::optional<GLuint>                                   maybe_previous_texture_id;
    std::optional<GLuint>                                   maybe_next_texture_id;
};
