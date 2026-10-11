#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) < 202002L
#error                                                                         \
"This file requires C++20 or later. Compile with -std=c++20 (or /std:c++20)."
#endif

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

using namespace std::string_view_literals;

#if defined(_MSC_VER)
constexpr std::string_view compiler_suffix = "msvc";
#else
constexpr std::string_view compiler_suffix = "gnu";
#endif

// Detect if we're running under Wine/Proton or native Windows.
std::string get_platform_name() {
    #if defined(_WIN32) || defined(_WIN64)

    // Check for Wine-specific NTDLL exports.
    #if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll && GetProcAddress(ntdll, "wine_get_version")) {
        // Proton consists of a Python script whose first startup check is for
        // this variable, otherwise it exits with an error. Since apps inherit
        // all env vars, checking for the same variable is the best detection.
        if (GetEnvironmentVariableA("STEAM_COMPAT_DATA_PATH", nullptr, 0) > 0) {
            return "proton"; // Running under Proton on Linux.
        }
        return "wine"; // Running under Wine on Linux.
    }
    #endif
    return "windows"; // Native Windows.

    #else
    return "linux"; // Non-Windows platform.
    #endif
}

// Safely print a path representation without throwing on unmappable codepages...
void log_path_safely(const std::filesystem::path &p, std::ostream &log) {
    try {
        // Uses u8string() instead of string(), since the latter attempts conversion
        // to single-byte via the current Windows "codepage" and throws if Unicode
        // characters can't be represented as one byte.
        // NOTE: On Linux, string() usually maps to UTF-8 but there's no guarantee.
        #if defined(__cpp_lib_char8_t) && __cpp_lib_char8_t >= 201811L
        auto u8 = p.u8string();
        std::string s(reinterpret_cast<const char *>(u8.data()), u8.size());
        #else
        auto s = p.u8string();
        #endif

        // ASCII/hex-escaped representation.
        log << "hex: [";
        for (unsigned char c : s) {
            if (c >= 32 && c <= 126)
                log << c;
            else
                log << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                    << (int)c << std::dec;
        }
        log << "]";

        // UTF-8 representation.
        log << " u8string: ["
            << s
            << "]";
    } catch (...) {
        log << "<u8string failed>";
    }
}

int main() {
    std::string platform = get_platform_name();
    std::string log_filename = std::string("scanner_")
        .append(platform)
        .append("_")
        .append(compiler_suffix)
        .append(".log");
    std::ofstream log(log_filename);
    if (!log.is_open()) {
        std::cerr << "Failed to open log file: '" << log_filename << "'.\n";
        return 2;
    }

    log << "=== Scanning all files to check for memory corruption. ===\n";

    const std::filesystem::path folder = "Data";

    std::error_code ec;
    std::filesystem::directory_iterator iter(folder, ec);

    if (ec) {
        log << "CRITICAL: directory_iterator failed to open 'Data': "
            << ec.message() << " (value: " << ec.value() << ")\n";
        log.flush();
        return 1;
    }

    size_t count = 0;
    size_t configs_found = 0;
    size_t files_tested = 0;
    size_t read_failures = 0;
    size_t exception_count = 0;

    try {
        for (const auto &entry : iter) {
            count++;
            try {
                std::error_code entry_ec;

                // Skip non-existent items or items with empty names/paths.
                if (!entry.exists(entry_ec) || entry.path().empty()) {
                    /*
                     * The Proton filename corruption bug always ends up in this branch,
                     * except when a corrupt, fake `/` forward slash has been encountered,
                     * which then makes the `exists()` calls always return true.
                     * SEE the README for full details.
                     */
                    log << "[" << count << "] SKIPPING MISSING OR EMPTY ITEM: ";
                    log_path_safely(entry.path(), log);
                    log << "\n";
                    continue;
                }

                // Skip directories.
                bool is_dir = entry.is_directory(entry_ec);
                if (entry_ec || is_dir) {
                    log << "[" << count << "] SKIPPING DIRECTORY: ";
                    log_path_safely(entry.path(), log);
                    log << "\n";
                    log.flush();
                    continue;
                }

                // Only test regular files (or symlinks pointing to regular files).
                bool is_reg = entry.is_regular_file(entry_ec);
                if (!entry_ec && is_reg) {
                    files_tested++;

                    // Print every filename as hex to help us find where
                    // the corruption happens.
                    log << "[" << count << "] Scanning file: ";
                    log_path_safely(entry.path(), log);
                    log << "\n";
                    log.flush();

                    // Attempt to open and read the first few bytes.
                    std::ifstream test_file(entry.path(), std::ios::binary);
                    if (!test_file.good()) {
                        read_failures++;
                        log << "[" << count << "] READ FAILED (bad stream on open): ";
                        log_path_safely(entry.path(), log);
                        log << "\n";
                        log.flush();
                    } else {
                        char buf[8] = {0};
                        test_file.read(buf, sizeof(buf));
                        // Check if stream hit a hard I/O failure (empty 0-byte
                        // files trigger EOF, not fail).
                        if (test_file.bad()) {
                            read_failures++;
                            log << "[" << count << "] READ FAILED (bad bit set after read): ";
                            log_path_safely(entry.path(), log);
                            log << "\n";
                            log.flush();
                        }
                    }
                } else {
                    log << "[" << count << "] SKIPPING FILE READ-TEST (not regular file): ";
                    log_path_safely(entry.path(), log);
                    log << "\n";
                    log.flush();
                }

                // Look for SoundRecordDistributor mod config files.
                const auto ext = entry.path().extension().u8string();
                if (ext != u8".json" && ext != u8".jsonc" && ext != u8".yaml") {
                    continue;
                }

                const auto stem = entry.path().stem().u8string();
                if (!stem.ends_with(u8"_SRD")) {
                    continue;
                }

                configs_found++;

                log << "[" << count << "] (" << configs_found
                    << ") Config found: ";
                log_path_safely(entry.path(), log);
                log << "\n";
                log.flush();

            } catch (const std::exception &e) {
                exception_count++;
                log << "[" << count << "] Exception on entry ";
                log_path_safely(entry.path(), log);
                log << ": " << e.what() << "\n";
                log.flush();
            }
        }
    } catch (const std::exception &e) {
        exception_count++;
        log << "CRITICAL: Exception on directory_iterator: " << e.what() << "\n";
        log.flush();
    }

    log << "=== Finished scanning ===\n"
        << "Total items enumerated: " << count << "\n"
        << "Regular files tested:   " << files_tested << "\n"
        << "Read failures:          " << read_failures << "\n"
        << "Exception errors:       " << exception_count << "\n"
        << "Configs found:          " << configs_found << "\n";
    log.flush();

    std::cout << "Scan finished. Log saved to: '" << log_filename << "'.\n";

    return 0;
}
