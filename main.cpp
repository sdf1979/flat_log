#include <iostream>
#include <vector>
#include <mutex>
#include <future>
#include <semaphore>
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

mutex coutMutex;

static wstring error_str(error_code& ec) {
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

SimdSupport::SimdLevel getSimdLevel(const ArgumentParser& arguments) {
    
    wstring simd_level_wstr = arguments.GetSimd();

    SimdSupport::SimdLevel simd_level;
    if (simd_level_wstr == L"auto") {
        SimdSupport sp;
        simd_level = sp.BestLevel();
    }
    else {
        simd_level = SimdSupport::StringToSimdLevel(simd_level_wstr);
    }

    return simd_level;
}

vector<fs::path> getLogFiles(const wstring& path) {
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

size_t convertFile(const fs::path& file, FlatLog::Mode mode, size_t chank_size, SimdSupport::SimdLevel simd_level) {
    auto start = chrono::high_resolution_clock::now();

    FlatLog flat_log(file.string());
    error_code ec;
    if (!flat_log.Open(ec)) {
        {
            lock_guard<mutex> lock(coutMutex);
            wcout << L"Error: file '" << file.wstring() << L"' not open (" << error_str(ec) << L")" << endl;
        }
        return 0;
    }

    if (flat_log.FileSize() <= 3) {
        {
            lock_guard<mutex> lock(coutMutex);
            wcout << L"File '" << file.wstring() << L"'is empty, skipping" << endl;
        }
        return 0;
    }

    flat_log.SetSimdLevel(simd_level);
       
    if (!flat_log.ProcessData(mode, chank_size, ec)) {
        lock_guard<mutex> lock(coutMutex);
        wcout << error_str(ec) << endl;
        return 0;
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
    
    {
        lock_guard<mutex> lock(coutMutex);
        wcout << L"file '" << file.wstring() << L"': " << flat_log.FileSize() << L" bytes in " << duration.count() << L" microseconds" << endl;
    }

    return flat_log.FileSize();
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
    wstring er;
    if (!arguments.Parse(argc, argv, er)) {
        wcout << er << endl;
        return 0;
    }

    if (arguments.IsHelp()) {
        SimdSupport sp;
        wcout << sp.ToString() << endl << arguments.Help() << endl;
        return 0;
    }

    wstring path = arguments.GetPath();
    if (path.empty()) {
        wcout << "Error: the '-P [--path]' parameter is empty. Specify a file or directory." << endl;
        return 0;
    }

    SimdSupport::SimdLevel simd_level = getSimdLevel(arguments);
    FlatLog::Mode mode = (arguments.GetMode() == L"flat" ? FlatLog::Mode::Flat : FlatLog::Mode::Unflat);
    
    wcout << L"SIMD: " << SimdSupport::SimdLevelToString(simd_level)
        << L"; Chank: " << arguments.GetChank() << L"GB"
        << L"; Mode=" << arguments.GetMode() << L";"
        << L"Thread=" << arguments.GetCountThread() << endl;

    atomic<size_t> all_size{ 0 };
    auto start = chrono::high_resolution_clock::now();

    vector<fs::path> files = getLogFiles(path);
    const size_t chank_size = arguments.GetChank() * 1024 * 1024 * 1024;

    int maxThreads = arguments.GetCountThread();
    counting_semaphore<> semaphore(maxThreads);
    std::vector<std::future<size_t>> futures;
    for (const auto& file : files) {
        semaphore.acquire();

        futures.push_back(std::async(std::launch::async,
            [&semaphore, &all_size, file, mode, chank_size, simd_level]() -> size_t {
                try {
                    size_t size = convertFile(file, mode, chank_size, simd_level);
                    all_size += size;
                    semaphore.release();
                    return size;
                }
                catch (...) {
                    semaphore.release();
                    return 0;
                }
            }));
    }

    for (auto& future : futures) {
        future.get();
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start);

    wcout << L"All in files: " << all_size << L" bytes in " << duration.count() << L" microseconds" << endl;
    return 0;
}