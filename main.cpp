#include <iostream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include "src/flat_log.h"
#include "src/simd_support.h"
#include "src/argument_parser.h"

using namespace std;

using FlatLog = soldy::FlatLog;
using SimdSupport = soldy::SimdSupport;
using ArgumentParser = soldy::ArgumentParser;
namespace fs = std::filesystem;

static std::wstring error_str(std::error_code& ec) {
    char* old_locale = std::setlocale(LC_ALL, nullptr);
    std::setlocale(LC_ALL, "en_US.UTF-8");

    std::string category = ec.category().name();

    std::wstring error_str;
    error_str
        .append(L"Error: ").append(std::wstring(ec.message().begin(), ec.message().end()))
        .append(L" (code: ").append(std::to_wstring(ec.value()))
        .append(L", category: ").append(std::wstring(category.begin(), category.end())).append(L")");

    std::setlocale(LC_ALL, old_locale);

    return error_str;
}

std::vector<fs::path> getLogFiles(const std::wstring& path) {
    std::vector<fs::path> logFiles;

    try {
        fs::path filePath(path);

        if (!fs::exists(filePath)) {
            std::wcout << "Error: the directory or file '" << path << "' does not exist." << std::endl;
            return logFiles;
        }

        if (fs::is_directory(filePath)) {
            for (const auto& entry : fs::recursive_directory_iterator(filePath)) {
                if (fs::is_regular_file(entry) &&
                    entry.path().extension() == ".log") {
                    logFiles.push_back(entry.path());
                }
            }
        }
        else if (fs::is_regular_file(filePath) && filePath.extension() == ".log") {
            logFiles.push_back(filePath);
        }
    }
    catch (const std::exception& ex) {
        std::wcout << "Error: could not retrieve log files due to " << ex.what() << std::endl;
    }

    return logFiles;
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
    auto cur_mode_out = _setmode(_fileno(stdout), _O_U16TEXT);
    auto cur_mode_in = _setmode(_fileno(stdin), _O_U16TEXT);
    auto cur_mode_err = _setmode(_fileno(stderr), _O_U16TEXT);
#else
int main(int argc, char* argv[], char* envp[]) {
#endif
    ArgumentParser arguments;
    std::wstring er;
    if (!arguments.Parse(argc, argv, er)) {
        std::wcout << er << std::endl;
        return 0;
    }

    if (arguments.IsHelp()) {
        SimdSupport sp;
        std::wcout << sp.ToString() << std::endl;
        std::wcout << arguments.Help() << std::endl;
        return 0;
    }

    std::wstring path = arguments.GetPath();
    if (path.empty()) {
        std::wcout << "Error: the '-P [--path]' parameter is empty. Specify a file or directory." << std::endl;
        return 0;
    }

    std::wstring simd_level_wstr = arguments.GetSimd();
    
    SimdSupport::SimdLevel simd_level;
    if (simd_level_wstr == L"auto") {
        SimdSupport sp;
        simd_level = sp.BestLevel();
    }
    else {
        simd_level = SimdSupport::StringToSimdLevel(simd_level_wstr);
    }
    std::wstring mode = arguments.GetMode();

    const size_t chank_size = arguments.GetChank() * 1024 * 1024 * 1024;
    std::vector<fs::path> files = getLogFiles(path);

    std::wcout << L"SIMD: " << SimdSupport::SimdLevelToString(simd_level)
        << L"; Chank: " << arguments.GetChank() << L"GB"
        << L"; Mode=" << mode << L";" << std::endl;

    size_t all_size = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& file : files) {

        auto start = std::chrono::high_resolution_clock::now();

        FlatLog flat_log(file.string());
        std::error_code ec;
        if (!flat_log.Open(ec)) {
            std::wcout << L"Error: file '" << file.wstring() << L"' not open (" << error_str(ec) << L")" << std::endl;
            continue;
        }

        if (flat_log.FileSize() <= 3) {
            std::wcout << L"File '" << file.wstring() << L"'is empty, skipping" << std::endl;
            continue;
        }

        if (!simd_level_wstr.empty()) {
            flat_log.SetSimdLevel(simd_level);
        }

        std::wcout << L"file '" << file.wstring() << L"': " << flat_log.FileSize() << L" bytes in ";

        if (mode == L"flat" ? !flat_log.ProcessData(FlatLog::Mode::Flat, chank_size, ec) : !flat_log.ProcessData(FlatLog::Mode::Unflat, chank_size, ec)) {
            std::wcout << error_str(ec) << std::endl;
            continue;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        all_size += flat_log.FileSize();

        std::wcout << duration.count() << L" microseconds" << std::endl;

    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::wcout << L"All in files: " << all_size << L" bytes in " << duration.count() << L" microseconds" << std::endl;
    return 0;
}