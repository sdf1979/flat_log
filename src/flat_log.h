#pragma once

#include <filesystem>
#include <string>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <immintrin.h>
#include "mapped_file.h"
#include "simd_support.h"

#ifdef _WIN32
#include <intrin.h>
#define CTZ64 _tzcnt_u64
#define CTZ32 _tzcnt_u32
#else
#define CTZ64 __builtin_ctzll
#define CTZ32 __builtin_ctz
#endif

namespace soldy {
		
	class FlatLog {
	private:
		static const char CR = '\r';
		static const char LF = '\n';
		static const char CHANGE_CR = 0x01;
		static const char CHANGE_LF = 0x02;

		std::filesystem::path file_path_;
		MappedFile mapped_file_;
		SimdSupport simd_support_;
		SimdSupport::SimdLevel simd_level_;
		inline void flat_chank_512(char* ch, size_t size, size_t block_size);
		inline void unflat_chank_512(char* ch, size_t size, size_t block_size);
		inline void flat_chank_256(char* ch, size_t size, size_t block_size);
		inline void unflat_chank_256(char* ch, size_t size, size_t block_size);
		inline void flat_chank_none(char* ch, size_t size, size_t block_size);
		inline void unflat_chank_none(char* ch, size_t size, size_t block_size);
		inline bool is_new_event_512(char* ch);
		inline bool is_new_event_256(char* ch);
		inline bool is_new_event(char* ch);
		void flat_remainder(char* ch, size_t size);
	public:
		enum class Mode {
			Flat,
			Unflat
		};
		explicit FlatLog(const std::string& path_str);
		bool Open(std::error_code& ec);
		bool ProcessData(Mode mode, size_t chank_size, std::error_code& ec);
		void SetSimdLevel(SimdSupport::SimdLevel simd_level);
		size_t FileSize() { return mapped_file_.FileSize(); }
	};

}