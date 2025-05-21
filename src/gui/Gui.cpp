#include "Gui.hpp"

#include <ranges>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Gui
{

auto init_window(const std::string& title, const int width, const int height) -> std::optional<GLFWwindow*>
{
    glfwSetErrorCallback(glfw_error_callback);

    if (glfwInit() == 0) {
        std::println(stderr, "[ERROR] (GLFW) Failed to initialize");
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
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

    return window;
}

void shutdown(GLFWwindow* window)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void new_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void load_default_fonts(const float regular_size, const float bold_size)
{
    auto& io = ImGui::GetIO();
    io.Fonts->Clear();

    regular_font   = io.Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", regular_size);
    io.FontDefault = regular_font;

    bold_font = io.Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", bold_size);
}

void center_content_horizontally(const float content_width)
{
    const auto spacing         = ImGui::GetStyle().ItemSpacing.x;
    const auto total_width     = content_width + spacing;
    const auto available_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((available_width - total_width) * 0.5F);
}

auto grid_layout_calc_size(const std::size_t rows, const std::size_t cols, const ImVec2& available_space) -> ImVec2
{
    const auto spacing = ImGui::GetStyle().ItemSpacing;

    return {
        (available_space.x - (spacing.x * 2)) / static_cast<float>(cols),
        (available_space.y - (spacing.y * 2)) / static_cast<float>(rows),
    };
}

void toast(
  const std::string&                 message,
  ToastPosition                      position,
  const std::chrono::duration<float> duration,
  ToastLevel                         level
)
{
    ToastManager::add(Toast {
      .message  = message,
      .duration = duration,
      .level    = level,
      .position = position,
    });
}

[[nodiscard]] auto input_text_popup(const std::string& label, bool& condition) -> std::optional<std::string>
{
    auto center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_Appearing);

    std::string           result;
    std::array<char, 256> buffer {};
    ImGui::OpenPopup("##InputPopup");
    if (ImGui::BeginPopupModal("##InputPopup", &condition, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetKeyboardFocusHere();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            condition = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return std::nullopt;
        }

        ImGui::PushFont(bold_font);
        Gui::text("{}: ", label);
        ImGui::SameLine();
        ImGui::PopFont();

        if (ImGui::InputText("##InputText", buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            result    = std::string(buffer.data());
            condition = false;

            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return result;
        }

        ImGui::EndPopup();
    }

    return std::nullopt;
}

namespace Plotting
{

void line(const std::string& label, const RingBuffer& buffer, LineFlags flags)
{
    ImPlot::PlotLine(
      label.c_str(),
      &buffer[0].x,
      &buffer[0].y,
      buffer.size(),
      std::to_underlying(flags),
      buffer.offset(),
      2 * sizeof(float)
    );
}


void bars(const std::span<const std::string> labels, const std::span<const double> values)
{
    constexpr static auto BAR_WIDTH = 0.2F;

    std::vector<const char*> labels_cstr;
    for (const auto& label : labels) { labels_cstr.push_back(label.c_str()); }

    std::vector<double> positions {};
    positions.reserve(labels.size());
    for (std::size_t pos = 0; pos < labels.size(); ++pos) { positions.push_back(static_cast<double>(pos)); }

    ImPlot::SetupAxisTicks(ImAxis_X1, positions.data(), static_cast<int>(positions.size()), nullptr);

    const auto point = std::views::zip(positions, values);
    for (const auto& [idx, coords] : std::views::zip(std::views::iota(0UL), point)) {
        const auto& [x, y] = coords;
        ImPlot::PushStyleColor(
          ImPlotCol_Fill, ImPlot::GetColormapColor(static_cast<int>(idx) % ImPlot::GetColormapSize())
        );
        ImPlot::PlotBars(labels_cstr[idx], &x, &y, 1, BAR_WIDTH);
        ImPlot::PopStyleColor();
    }

    // Check for hover and display tooltip to show value
    ImPlotPoint mouse_position = ImPlot::GetPlotMousePos();
    for (const auto& [idx, position] : std::views::zip(std::views::iota(0UL), positions)) {
        const auto half_width = BAR_WIDTH / 2.0;
        const auto x_range    = ImPlotRange(position - half_width, position + half_width);
        const auto y_range    = ImPlotRange(0, values[idx]);

        if (x_range.Contains(mouse_position.x) && y_range.Contains(mouse_position.y)) {
            if (ImPlot::IsPlotHovered()) { Gui::tooltip("{}", values[idx]); }
        }
    }
}

} // namespace Plotting

void draw_call(GLFWwindow* window, const ImVec4& clear_color)
{
    ToastManager::render();
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

} // namespace Gui
