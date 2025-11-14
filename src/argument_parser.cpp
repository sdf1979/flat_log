#include "argument_parser.h"

namespace soldy {

#ifdef _WIN32
	bool ArgumentParser::Parse(int argc, wchar_t* argv[], std::wstring& er) {
		bool is_succes = true;
		for (int i = 1; i < argc; ++i) {
			is_succes = parseArg(argv[i], er) && is_succes;
		}
		return is_succes;
	}
#else
	bool ArgumentParser::Parse(int argc, char* argv[], std::wstring& er) {
		const char* cur_locale = std::setlocale(LC_ALL, nullptr);
		const char* new_locale = std::setlocale(LC_ALL, "");
		
		bool is_succes = true;
		for (int i = 1; i < argc; ++i) {
			std::string arg = argv[i];
			size_t len = mbstowcs(nullptr, arg.c_str(), 0);
			std::wstring argw(len, L'\0');
			mbstowcs(&argw[0], arg.c_str(), len);
			is_succes = parseArg(argw, er) && is_succes;
		}

		std::setlocale(LC_ALL, cur_locale);
		return is_succes;
	}
#endif

	bool ArgumentParser::IsHelp() const {
		if (arguments_.empty()) {
			return true;
		}
		auto it = arguments_.find(L"help");
		if (it != arguments_.end()) {
			return true;
		}
		return false;
	}

	std::wstring ArgumentParser::Help() const {
		std::wstring help =
			L"All options:\n"
			L"  -P [ --path  ] arg          Full path to the directory with logs or log file.\n"
			L"  -C [ --chank ] arg (=4)     The chunk size in gigabytes when mapping a file into memory.\n"
			L"                              Available values : 1, 2, 4, 8, 16, 32, 64, 128, 256.\n"
			L"  -M [ --mode  ] arg (=flat)  Launch mode, flat - replace line breaks in a multi-line event with\n"
			L"                              service characters, unflat - reverse transformation.\n"
			L"  -S [ --simd  ] arg (=auto)  The option to use SIMD processor instructions.\n"
			L"                              Possible values : auto, avx512, avx2, none.\n"
			L"  -H [ --help  ]              Produce help message\n";
		
		const char* old_locale = setlocale(LC_ALL, nullptr);
		
		return help;
	}

	std::wstring ArgumentParser::GetMode() const {
		return get(L"mode", L"flat");
	}

	std::wstring ArgumentParser::GetPath() const {
		return get(L"path");
	}

	std::wstring ArgumentParser::GetSimd() const {
		return get(L"simd", L"auto");
	}

	size_t ArgumentParser::GetChank() const {
		std::wstring chankw = get(L"chank", L"4");
		return static_cast<size_t>(std::stoull(chankw));
	}

	bool ArgumentParser::parseArg(const std::wstring& arg, std::wstring& er) {
		if (arg.starts_with(L"-") || arg.starts_with(L"--")) {
			
			std::wstring key, value;
			size_t len_prefix = arg.starts_with(L"--") ? 2 : 1;

			size_t pos = arg.find('=');
			if (pos != std::string::npos) {
				key = arg.substr(len_prefix, pos - len_prefix);
				value = arg.substr(pos + 1);
			}
			else {
				key = arg.substr(len_prefix);
			}

			if (key == L"P" || key == L"path") {
				key = L"path";
			} else if (key == L"M" || key == L"mode") {
				key = L"mode";
				if (!(value == L"flat" || value == L"unflat")) {
					er.append(L"Invalid value '").append(value).append(L"' for parameter '- M[--mode]'.\n");
					return false;
				}
			} else if (key == L"S" || key == L"simd") {
				key = L"simd";
				if (!(value == L"auto" || value == L"avx512" || value == L"avx2" || value == L"none")) {
					er.append(L"Invalid value '").append(value).append(L"' for parameter '-S [--simd]'.\n");
					return false;
				}
			} else if (key == L"C" || key == L"chank") {
				key = L"chank";
				if (!(value == L"1" || value == L"2" || value == L"4" || value == L"8" || value == L"16"
					|| value == L"32" || value == L"64" || value == L"128" || value == L"256")) {
					er.append(L"Invalid value '").append(value).append(L"' for parameter '-C [--chank]'.\n");
					return false;
				}
			} else if (key == L"H" || key == L"help") {
				key = L"help";
			} else {
				er.append(L"Unknown parameter '").append(key).append(L"'.\n");
				return false;
			}

			arguments_[key] = value;
			return true;
		}
		else {
			er.append(L"The argument '").append(arg).append(L"' does not begin with the required '-' or '--' characters.");
			return false;
		}
		return false;
	}

	std::wstring ArgumentParser::get(const std::wstring& key, const std::wstring& defaultValue) const {
		auto it = arguments_.find(key);
		if (it != arguments_.end()) {
			return it->second;
		}
		return defaultValue;
	}



}