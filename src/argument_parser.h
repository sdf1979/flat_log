#pragma once

#include <string>
#include <unordered_map>
#include <clocale>
#include <locale>

namespace soldy {

	class ArgumentParser {
	private:
		std::unordered_map<std::wstring, std::wstring> arguments_;
		bool parseArg(const std::wstring& arg, std::wstring& er);
		std::wstring get(const std::wstring& key, const std::wstring& defaultValue = L"") const;
	public:
		ArgumentParser() = default;
#ifdef _WIN32
		bool Parse(int argc, wchar_t* argv[], std::wstring& er);
#else
		bool Parse(int argc, char* argv[], std::wstring& er);
#endif
		std::wstring GetMode() const;
		std::wstring GetPath() const;
		std::wstring GetSimd() const;
		size_t GetChank() const;
		bool IsHelp() const;
		std::wstring Help() const;
	};

}