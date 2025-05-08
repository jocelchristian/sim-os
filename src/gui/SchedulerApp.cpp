#include <filesystem>
#include <fstream>
#include <sstream>
#include <print>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

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
  : window { window }, sim { sim }
{}

SchedulerApp::~SchedulerApp()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void SchedulerApp::render()
{
    ImVec4 clear_color = ImVec4(0.45, 0.55, 0.60, 1.00);

    while (!quit) {
        if (glfwWindowShouldClose(window) == 1) { quit = true; }

        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) { continue; }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // TODO: handle input and then display the status
        // of the queues

        // Main window
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0);
            ImGui::SetNextWindowPos(ImVec2(0.0, 0.0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("sim-os: scheduler", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
            ImGui::End();
            ImGui::PopStyleVar(1);
        }

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

auto main() -> int
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

    auto sim = std::make_shared<Simulations::Scheduler>(Simulations::Scheduler(round_robin_scheduler));

    constexpr std::string_view script = "examples/scheduler/simple.sl";
    const auto                 path   = std::filesystem::path(script);
    auto                       file   = std::ifstream(path);
    if (!file) { std::println(stderr, "[ERROR] Unable to read file {}: {}", path.string(), strerror(errno)); }

    std::stringstream ss;
    ss << file.rdbuf();
    const auto file_content = ss.str();

    if (!Interpreter::Interpreter<Simulations::Scheduler>::eval(file_content, sim)) {
        std::println(stderr, "[ERROR] Could not correctly evaluate script {}", script);
    }

    auto app = Gui::SchedulerApp::create(sim);
    app->render();
}
