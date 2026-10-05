#include "application/platform_paths.hpp"

#include <cstdint>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>

#include <vector>
#endif

namespace tpc_slint::application {
namespace {
namespace fs = std::filesystem;

[[nodiscard]] fs::path executableDirectory() {
#ifdef _WIN32
    std::wstring buffer(260, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            break;
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return fs::path{buffer}.parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    (void)_NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        std::error_code error;
        const auto executable = fs::weakly_canonical(fs::path{buffer.data()}, error);
        return (error ? fs::path{buffer.data()} : executable).parent_path();
    }
#else
    std::error_code error;
    const auto executable = fs::read_symlink("/proc/self/exe", error);
    if (!error) {
        return executable.parent_path();
    }
#endif
    return fs::current_path();
}
}  // namespace

fs::path settingsPath() {
    return executableDirectory() / "Settings" / "settings.json";
}

}  // namespace tpc_slint::application
