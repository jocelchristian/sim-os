#include "imgui.h"
#include <filesystem>
#include <fstream>
#include <GL/gl.h>
#include <print>
#include <sstream>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "lang/Interpreter.hpp"
#include "os/Os.hpp"
#include "SchedulerApp.hpp"
#include "simulations/Scheduler.hpp"

static void glfw_error_callback(int error, const char* description)
{
    std::print(stderr, "[ERROR] GLFW Error ({}): {}\n", error, description);
}

namespace Gui
{

auto SchedulerApp::create(const std::shared_ptr<Simulations::Scheduler>& sim) -> std::unique_ptr<SchedulerApp>
{
    glfwSetErrorCallback(glfw_error_callback);

    if (glfwInit() == 0) {
        std::println(stderr, "[ERROR] (GLFW) Failed to initialize");
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "sim-os: scheduler", nullptr, nullptr);
    if (window == nullptr) {
        std::println(stderr, "[ERROR] (Glfw) failed to create window");
        return nullptr;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);

    return std::unique_ptr<SchedulerApp>(new SchedulerApp { window, sim });
}

SchedulerApp::SchedulerApp(GLFWwindow* window, const std::shared_ptr<Simulations::Scheduler>& sim)
  : window { window },
    sim { sim }
{}

SchedulerApp::~SchedulerApp()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

template<typename... Args>
static void draw_text_formatted(std::format_string<Args...> fmt, Args&&... args)
{
    ImGui::TextUnformatted(std::format(fmt, std::forward<Args>(args)...).c_str());
}

static void draw_child_title(const std::string& title, const ImVec2& child_size)
{
    constexpr static auto title_height = 24.0F;
    const auto            title_size   = ImVec2(child_size.x, title_height);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
    ImGui::BeginChild(std::format("{}_title", title).c_str(), title_size, 0);

    ImGui::SetCursorPosX(8.0F);
    ImGui::SetCursorPosY((title_height - ImGui::GetTextLineHeight()) * 0.5F);
    ImGui::TextUnformatted(title.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
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
                draw_text_formatted("{}", event.kind);

                ImGui::TableSetColumnIndex(1);
                draw_text_formatted("{}", event.duration);
            }

            ImGui::EndTable();
        }
    }
}

static std::optional<GLuint> load_texture(const std::filesystem::path& path)
{
    int           width    = 0;
    int           height   = 0;
    int           channels = 0;
    std::uint8_t* bytes    = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (bytes == nullptr) {
        std::println(stderr, "[ERROR] (stb) Failed to load file: {}", path.string());
        return std::nullopt;
    }

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(bytes);
    return texture_id;
}

void SchedulerApp::draw_control_buttons()
{
    // TODO: implement previous

    constexpr static auto button_size = ImVec2(16, 16);

    const auto spacing         = ImGui::GetStyle().ItemSpacing.x;
    const auto total_width     = (button_size.x * 2.0F) + spacing;
    const auto available_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((available_width - total_width) * 0.5F);

    // Fallback
    if (!maybe_previous_texture_id || !maybe_next_texture_id) {
        if (ImGui::Button("Previous")) {}

        ImGui::SameLine();

        if (ImGui::Button("Next") && !sim->complete()) { sim->step(); }

        return;
    }

    auto* const previous_texture_id = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_previous_texture_id));
    if (ImGui::ImageButton(previous_texture_id, button_size)) {}

    ImGui::SameLine();

    auto* const next_texture_id = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(*maybe_next_texture_id));
    if (ImGui::ImageButton(next_texture_id, button_size) && !sim->complete()) { sim->step(); }
}

static void draw_process(const Simulations::Scheduler::ProcessPtr& process)
{
    if (process == nullptr) { return; }

    if (ImGui::BeginChild("Scrollable Process", ImVec2(0, 0), 1, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        if (ImGui::CollapsingHeader(std::string { process->name }.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();

            draw_text_formatted("Pid: {}", process->pid);
            draw_text_formatted("Arrival Time: {}", process->arrival);

            draw_events_table(process->events);

            ImGui::Unindent();
        }

        ImGui::EndChild();
    }
}

static void draw_running_process(const Simulations::Scheduler::ProcessPtr& process, const ImVec2& child_size)
{
    draw_child_title("Running", child_size);

    if (ImGui::BeginChild("Scrollable Process", child_size, 1, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        if (process != nullptr) {
            if (ImGui::CollapsingHeader(std::string { process->name }.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();

                draw_text_formatted("Pid: {}", process->pid);
                draw_text_formatted("Arrival Time: {}", process->arrival);

                draw_events_table(process->events);

                ImGui::Unindent();
            }
        }
    }
    ImGui::EndChild();
}


static void draw_process_queue(
  const std::string_view                      title,
  const Simulations::Scheduler::ProcessQueue& processes,
  const ImVec2&                               child_size
)
{
    const auto null_terminated = std::string { title };

    draw_child_title(null_terminated, child_size);

    if (ImGui::BeginChild(null_terminated.c_str(), child_size, 1, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        for (const auto& process : processes) { draw_process(process); }
    }

    ImGui::EndChild();
}

void SchedulerApp::draw_statistics(const ImVec2& child_size)
{
    const auto draw_table_element = [](const auto& key, const auto& value) -> void {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        draw_text_formatted("{}", key);

        ImGui::TableSetColumnIndex(1);
        draw_text_formatted("{}", value);
    };

    draw_child_title("Stats", child_size);

    if (ImGui::BeginChild("Simulation Statistics", child_size, 1, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        if (ImGui::BeginTable("Stats Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

            // TODO: Add schedule policy
            draw_table_element("Ready queue size", sim->ready.size());
            draw_table_element("Waiting queue size", sim->waiting.size());
            draw_table_element("Arrival size", sim->processes.size());
            draw_table_element("Timer", sim->timer);

            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
}

void SchedulerApp::render()
{
    ImVec4 clear_color = ImVec4(0.094, 0.094, 0.094, 1.00);

    maybe_previous_texture_id = load_texture("resources/previous.png");
    maybe_next_texture_id     = load_texture("resources/next.png");

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

            const auto spacing         = ImGui::GetStyle().ItemSpacing.x;
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
            draw_running_process(sim->running, ImVec2(group_width, (group_height / 2.0F) - 16.0F));
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

} // namespace Gui

auto main(int argc, const char** argv) -> int
{
    const auto round_robin_scheduler = [quantum = 5UL](auto& sim) {
        if (sim.ready.empty()) { return; }

        const auto process = sim.ready.front();
        sim.ready.pop_front();
        sim.running = process;

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

    if (argc < 2) {
        std::println(stderr, "[ERROR] expected file path to simulation script");
        std::println("usage: scheduler <file.sl>");
    }

    const auto* const script_path          = argv[1];
    const auto        maybe_script_content = Util::read_entire_file(script_path);
    if (!maybe_script_content) { return 1; }

    auto sim = std::make_shared<Simulations::Scheduler>(round_robin_scheduler);
    if (!Interpreter::Interpreter<Simulations::Scheduler>::eval(*maybe_script_content, sim)) {
        std::println(stderr, "[ERROR] Could not correctly evaluate script {}", script_path);
    }

    auto app = Gui::SchedulerApp::create(sim);
    app->render();
}
