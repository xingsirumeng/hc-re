#pragma once
#include <cassert>
#include <filesystem>
#include <optional>
#include <string>

namespace xdg {

inline std::optional<std::string> env(char const *variable)
{
    char *p = std::getenv(variable);
    return p != nullptr ? std::optional{p} : std::nullopt;
}

namespace fs = std::filesystem;

inline fs::path home()
{
    // POSIX
    if (auto p = env("HOME"); p)
        return *p;

#ifdef _WIN32
    if (auto p = env("USERPROFILE"); p)
        return *p;
#endif

    throw std::runtime_error{"Cannot determine home directory: no appropriate "
                             "environment variable presents"};
}

inline fs::path cache_home()
{
    auto dir = env("XDG_CACHE_HOME");
    return dir.has_value() ? fs::path{*dir} : home() / ".cache";
}

inline fs::path config_home()
{
    auto dir = env("XDG_CONFIG_HOME");
    return dir.has_value() ? fs::path{*dir} : home() / ".config";
}

inline fs::path data_home()
{
    auto dir = env("XDG_DATA_HOME");
    return dir.has_value() ? fs::path{*dir} : home() / ".local" / "share";
}

} // namespace xdg
