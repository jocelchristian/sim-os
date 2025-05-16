#pragma once

#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <print>
#include <span>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <implot.h>
#include <string>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static void glfw_error_callback(int error, const char* description)
{
    std::print(stderr, "[ERROR] GLFW Error ({}): {}\n", error, description);
}

namespace Gui
{

constexpr static auto GLSL_VERSION = "#version 330";

enum class ChildFlags : std::uint8_t
{
    None   = 0,
    Border = 1 << 0,
};

[[nodiscard]] constexpr static auto operator|(ChildFlags lhs, ChildFlags rhs) -> ChildFlags
{
    return static_cast<ChildFlags>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

enum class WindowFlags : std::uint16_t
{
    None                    = 0,
    NoTitleBar              = 1 << 0,
    NoResize                = 1 << 1,
    NoMove                  = 1 << 2,
    NoScrollbar             = 1 << 3,
    NoCollapse              = 1 << 5,
    AlwaysVerticalScrollbar = 1 << 14,
    NoDecoration            = NoTitleBar | NoResize | NoScrollbar | NoCollapse,
};

[[nodiscard]] constexpr static auto operator|(WindowFlags lhs, WindowFlags rhs) -> WindowFlags
{
    return static_cast<WindowFlags>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

[[nodiscard]] static auto init_window(const std::string& title, const int width, const int height)
  -> std::optional<GLFWwindow*>
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

static void shutdown(GLFWwindow* window)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

template<typename... Args>
static void text(std::format_string<Args...> fmt, Args&&... args)
{
    ImGui::TextUnformatted(std::format(fmt, std::forward<Args>(args)...).c_str());
}

template<std::invocable<ImVec2> Callback>
static void title(const std::string& title, const ImVec2& child_size, Callback&& callback)
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

    const auto spacing = ImGui::GetStyle().ItemSpacing.y;
    std::invoke(std::forward<Callback>(callback), ImVec2(child_size.x, child_size.y - title_height - spacing));
}

[[nodiscard]] static std::optional<GLuint> load_texture(const std::filesystem::path& path)
{
    int           width    = -1;
    int           height   = -1;
    int           channels = -1;
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

template<std::invocable Callback>
static void group(Callback&& callback)
{
    ImGui::BeginGroup();
    std::invoke(std::forward<Callback>(callback));
    ImGui::EndGroup();
}

template<std::invocable Callback>
static void button(const std::string& label, Callback&& callback)
{
    if (ImGui::Button(label.c_str(), ImVec2(0, 0))) { std::invoke(std::forward<Callback>(callback)); }
}

template<std::invocable Callback>
static void button(const std::string& label, const ImVec2& size, Callback&& callback)
{
    if (ImGui::Button(label.c_str(), size)) { std::invoke(std::forward<Callback>(callback)); }
}

template<std::invocable Callback>
static void image_button(ImTextureID texture_id, const ImVec2& size, Callback&& callback)
{
    if (ImGui::ImageButton(texture_id, size)) { std::invoke(std::forward<Callback>(callback)); }
}

static void center_content_horizontally(const float content_width)
{
    const auto spacing         = ImGui::GetStyle().ItemSpacing.x;
    const auto total_width     = content_width + spacing;
    const auto available_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((available_width - total_width) * 0.5F);
}

template<std::invocable Callback>
static void child(const std::string& title, ChildFlags child_flags, WindowFlags window_flags, Callback&& callback)
{
    if (ImGui::BeginChild(
          title.c_str(), ImVec2(0, 0), std::to_underlying(child_flags), std::to_underlying(window_flags)
        )) {
        std::invoke(std::forward<Callback>(callback));
    }

    ImGui::EndChild();
}

template<std::invocable Callback>
static void child(
  const std::string& title,
  const ImVec2&      size,
  ChildFlags         child_flags,
  WindowFlags        window_flags,
  Callback&&         callback
)
{
    if (ImGui::BeginChild(title.c_str(), size, std::to_underlying(child_flags), std::to_underlying(window_flags))) {
        std::invoke(std::forward<Callback>(callback));
    }

    ImGui::EndChild();
}

template<std::invocable Callback>
static void window(const std::string& title, WindowFlags window_flags, Callback&& callback)
{
    ImGui::Begin(title.c_str(), nullptr, std::to_underlying(window_flags));
    std::invoke(std::forward<Callback>(callback));
    ImGui::End();
}

static void new_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

static void draw_call(GLFWwindow* window, const ImVec4& clear_color)
{
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

enum class TableFlags : std::uint32_t
{
    RowBackground          = 1 << 6,
    BordersInnerHorizontal = 1 << 7,
    BordersOuterHorizontal = 1 << 8,
    BordersInnerVertical   = 1 << 9,
    BordersOuterVertical   = 1 << 10,
    BordersInner           = BordersInnerVertical | BordersInnerHorizontal,
    BordersOuter           = BordersOuterVertical | BordersOuterHorizontal,
    Borders                = BordersInner | BordersOuter,
};

[[nodiscard]] constexpr static auto operator|(TableFlags lhs, TableFlags rhs) -> TableFlags
{
    return static_cast<TableFlags>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

template<std::invocable Callback>
static void
  draw_table(const std::string& name, const std::span<const char* const> headers, TableFlags flags, Callback&& callback)
{
    if (ImGui::BeginTable(name.c_str(), headers.size(), std::to_underlying(flags))) {
        for (const auto& header : headers) { ImGui::TableSetupColumn(header); }
        ImGui::TableHeadersRow();
        std::invoke(std::forward<Callback>(callback));
        ImGui::EndTable();
    }
}

template<typename... Callbacks>
static void draw_table_row(Callbacks&&... callbacks)
{
    ImGui::TableNextRow();

    int column = 0;
    ((ImGui::TableSetColumnIndex(column++), std::invoke(std::forward<Callbacks>(callbacks))), ...);
}

enum class TreeNodeFlags : std::uint8_t
{
    DefaultOpen = 1 << 5,
};

[[nodiscard]] constexpr static auto operator|(TreeNodeFlags lhs, TreeNodeFlags rhs) -> TreeNodeFlags
{
    return static_cast<TreeNodeFlags>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

template<std::invocable Callback>
static void collapsing(const std::string& name, TreeNodeFlags flags, Callback&& callback)
{
    if (ImGui::CollapsingHeader(name.c_str(), std::to_underlying(flags))) {
        ImGui::Indent();
        std::invoke(std::forward<Callback>(callback));
        ImGui::Unindent();
    }
}

[[nodiscard]] static auto grid_layout_calc_size(
  const std::size_t rows,
  const std::size_t cols,
  const ImVec2&     available_space = ImGui::GetContentRegionAvail()
) -> ImVec2
{
    const auto spacing = ImGui::GetStyle().ItemSpacing;

    return {
        (available_space.x - (spacing.x * 2)) / static_cast<float>(cols),
        (available_space.y - (spacing.y * 2)) / static_cast<float>(rows),
    };
}

namespace Plotting
{

class [[nodiscard]] RingBuffer final
{
  public:
    explicit RingBuffer(int capacity = 2000)
      : capacity { capacity }
    {
        data.reserve(capacity);
    }

    void emplace_point(const float x, const float y)
    {
        if (data.size() < capacity) {
            data.push_back(ImVec2(x, y));
            return;
        }

        data[cursor] = ImVec2(x, y);
        cursor       = (cursor + 1) % capacity;
    }

    [[nodiscard]] auto operator[](const int index) const -> const ImVec2& { return data[index]; }

    [[nodiscard]] auto size() const -> int { return data.size(); }

    [[nodiscard]] auto offset() const -> int { return cursor; }

  private:
    int              capacity;
    int              cursor = 0;
    ImVector<ImVec2> data;
};

enum class AxisFlags : std::uint8_t
{
    None         = 0,
    NoTickMarks  = 1 << 2,
    NoTickLabels = 1 << 3,
};

[[nodiscard]] constexpr static auto operator|(AxisFlags lhs, AxisFlags rhs) -> AxisFlags
{
    return static_cast<AxisFlags>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

struct [[nodiscard]] PlotOpts final
{
    AxisFlags x_axis_flags;
    AxisFlags y_axis_flags;
    double    x_min;
    double    x_max;
    double    y_min;
    double    y_max;

    std::optional<std::string> x_label;
    std::optional<std::string> y_label;
    std::optional<ImVec4>      color;
    std::optional<float>       line_weight;
};

template<std::invocable Callback>
static void plot(const std::string& title, const ImVec2& size, const PlotOpts& opts, Callback&& callback)
{
    if (ImPlot::BeginPlot(title.c_str(), size)) {
        ImPlot::SetupAxes(
          opts.x_label.has_value() ? opts.x_label->c_str() : nullptr,
          opts.y_label.has_value() ? opts.y_label->c_str() : nullptr,
          std::to_underlying(opts.x_axis_flags),
          std::to_underlying(opts.y_axis_flags)
        );

        ImPlot::SetupAxisLimits(ImAxis_X1, opts.x_min, opts.x_max, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, opts.y_min, opts.y_max, ImGuiCond_Always);

        if (opts.color) { ImPlot::PushStyleColor(ImPlotCol_Line, opts.color.value()); }
        if (opts.line_weight) { ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, opts.line_weight.value()); }

        std::invoke(std::forward<Callback>(callback));

        if (opts.line_weight) { ImPlot::PopStyleVar(); }
        if (opts.color) { ImPlot::PopStyleColor(); }
        ImPlot::EndPlot();
    }
}

enum class LineFlags : std::uint8_t
{
    None = 0,
};

[[nodiscard]] constexpr static auto operator|(LineFlags lhs, LineFlags rhs) -> LineFlags
{
    return static_cast<LineFlags>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

static void line(const std::string& label, const RingBuffer& buffer, LineFlags flags)
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

} // namespace Plotting

} // namespace Gui
