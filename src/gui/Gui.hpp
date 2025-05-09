#pragma once

#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <print>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static void glfw_error_callback(int error, const char* description)
{
    std::print(stderr, "[ERROR] GLFW Error ({}): {}\n", error, description);
}

namespace Gui
{
constexpr static auto GLSL_VERSION = "#version 330";

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

template<typename... Args>
static void draw_text_formatted(std::format_string<Args...> fmt, Args&&... args)
{
    ImGui::TextUnformatted(std::format(fmt, std::forward<Args>(args)...).c_str());
}

template<std::invocable Callback>
static void draw_titled_child(const std::string& title, const ImVec2& child_size, Callback&& callback)
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

    std::invoke(std::forward<Callback>(callback));
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
} // namespace Gui
