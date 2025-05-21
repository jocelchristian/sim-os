#include "Util.hpp"

#include <fstream>
#include <random>
#include <sstream>

namespace Util
{

auto read_entire_file(const std::filesystem::path& file_path) -> std::optional<std::string>
{
    if (!std::filesystem::exists(file_path)) {
        std::println(stderr, "[ERROR] Unable to read file {}: No such file or directory", file_path.string());
        return std::nullopt;
    }

    if (!std::filesystem::is_regular_file(file_path)) {
        std::println(stderr, "[ERROR] Unable to read file {}: Not a regular file", file_path.string());
        return std::nullopt;
    }

    std::ifstream     file(file_path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void write_to_file(const std::filesystem::path& file_path, const std::string& content)
{
    std::ofstream file(file_path, std::ios::out | std::ios::trunc);
    file << content;
}

auto random_float() -> float
{
    std::random_device                    rd;
    std::mt19937                          gen(rd());
    std::uniform_real_distribution<float> dis(0.0F, 1.0F);
    return dis(gen);
}

auto random_natural(const std::size_t min, const std::size_t max) -> std::size_t
{
    if (max == 0) { return 0; }

    std::random_device                         rd;
    std::mt19937                               gen(rd());
    std::uniform_int_distribution<std::size_t> dis(min, max);
    return dis(gen);
}

} // namespace Util
