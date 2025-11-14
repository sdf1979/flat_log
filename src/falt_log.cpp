#include "flat_log.h"

namespace soldy {

	FlatLog::FlatLog(const std::string& path_str) : file_path_(path_str) {
		simd_level_ = simd_support_.BestLevel();
	}

	bool FlatLog::Open(std::error_code& ec) {
		return mapped_file_.OpenSequential(file_path_, ec);
	}

	bool FlatLog::ProcessData(Mode mode, size_t chank_size, std::error_code& ec) {
		
		const size_t file_size = mapped_file_.FileSize();
		const size_t block_size = (simd_level_ == SimdSupport::SimdLevel::None) ? 12 : simd_support_.BlockSize(simd_level_);
		const size_t delta_offset = chank_size - block_size;

		//Т.к. для анализа нужна информация из следующго блока, то обрабатываем не все блоки в выделенном MapRegion, а на один меньше
		//Начало следующего MapRegion сдвигаем на размер block_size от конца предыдушего
		//Так же мы проверяем предыдущий символ от '\n' и если '\n' попадет на начало блока будет ошибка, сдвигаем регион еще на один символ влево
		size_t delta_ofset_reg = 0;
		for (size_t offset = 0; offset < file_size; offset += delta_offset) {
			
			if (!mapped_file_.MapRegion(offset - delta_ofset_reg, (std::min)(chank_size + delta_ofset_reg, file_size - offset + delta_ofset_reg), ec)) {
				return false;
			}

			if (mode == Mode::Flat && simd_level_ == SimdSupport::SimdLevel::AVX512) {
				flat_chank_512(static_cast<char*>(mapped_file_.Data()) + delta_ofset_reg, mapped_file_.MapSize() - delta_ofset_reg, block_size);
			} else if (mode == Mode::Flat && simd_level_ == SimdSupport::SimdLevel::AVX2) {
				flat_chank_256(static_cast<char*>(mapped_file_.Data()) + delta_ofset_reg, mapped_file_.MapSize() - delta_ofset_reg, block_size);
			} else if (mode == Mode::Flat && simd_level_ == SimdSupport::SimdLevel::None) {
				flat_chank_none(static_cast<char*>(mapped_file_.Data()) + delta_ofset_reg, mapped_file_.MapSize() - delta_ofset_reg, block_size);
			} else if (mode == Mode::Unflat && simd_level_ == SimdSupport::SimdLevel::AVX512) {
				unflat_chank_512(static_cast<char*>(mapped_file_.Data()) + delta_ofset_reg, mapped_file_.MapSize() - delta_ofset_reg, block_size);
			} else if (mode == Mode::Unflat && simd_level_ == SimdSupport::SimdLevel::AVX2) {
				unflat_chank_256(static_cast<char*>(mapped_file_.Data()) + delta_ofset_reg, mapped_file_.MapSize() - delta_ofset_reg, block_size);
			} else if (mode == Mode::Unflat && simd_level_ == SimdSupport::SimdLevel::None) {
				unflat_chank_none(static_cast<char*>(mapped_file_.Data()) + delta_ofset_reg, mapped_file_.MapSize() - delta_ofset_reg, block_size);
			}

			delta_ofset_reg = 1;
		}

		//Нужно обработать данные в конце файла
		size_t not_processed_size = file_size % block_size + block_size;
		if (!mapped_file_.MapRegion(file_size - not_processed_size - delta_ofset_reg, not_processed_size + delta_ofset_reg, ec)) {
			return false;
		}
		flat_remainder(static_cast<char*>(mapped_file_.Data()) + delta_ofset_reg, mapped_file_.MapSize());
		
		return true;
	}

	void FlatLog::SetSimdLevel(SimdSupport::SimdLevel simd_level) {
		simd_level_ = simd_level;
	}

#ifdef __linux__
	__attribute__((target("avx512f,avx512bw")))
#endif
	inline void FlatLog::flat_chank_512(char* ch, size_t size, size_t block_size) {
		__m512i newline_mask = _mm512_set1_epi8(LF);
		char* end = ch + ((size / block_size) - 1) * block_size;
		for (; ch < end; ch += block_size) {
			__m512i block = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ch));
			uint64_t  mask = _mm512_cmpeq_epi8_mask(block, newline_mask);
			while (mask != 0) {
				// Находим позицию первого установленного бита
				size_t pos = CTZ64(mask);
				if (!is_new_event_512(ch + pos + 1)) {
					*(ch + pos) = CHANGE_LF;
					char& prev_ch = *(ch + pos - 1);
					if (prev_ch == CR) {
						prev_ch = CHANGE_CR;
					}
				}
				// Сбрасываем обработанный бит
				mask &= ~(1ULL << pos);
			}
		}
	}

#ifdef __linux__
	__attribute__((target("avx512f,avx512bw")))
#endif
	inline void FlatLog::unflat_chank_512(char* ch, size_t size, size_t block_size) {
		__m512i newline_mask = _mm512_set1_epi8(CHANGE_LF);
		char* end = ch + ((size / block_size) - 1) * block_size;
		for (; ch < end; ch += block_size) {
			__m512i block = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ch));
			uint64_t  mask = _mm512_cmpeq_epi8_mask(block, newline_mask);
			while (mask != 0) {
				// Находим позицию первого установленного бита
				size_t pos = CTZ64(mask);
				*(ch + pos) = LF;
				char& prev_ch = *(ch + pos - 1);
				if (prev_ch == CHANGE_CR) {
					prev_ch = CR;
				}
				// Сбрасываем обработанный бит
				mask &= ~(1ULL << pos);
			}
		}
	}

#ifdef __linux__
	__attribute__((target("avx2")))
#endif
	inline void FlatLog::flat_chank_256(char* ch, size_t size, size_t block_size) {
		__m256i newline_mask = _mm256_set1_epi8(LF);
		char* end = ch + ((size / block_size) - 1) * block_size;

		for (; ch < end; ch += block_size) {
			__m256i block = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ch));
			uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, newline_mask));

			while (mask != 0) {
				// Находим позицию первого установленного бита
				size_t pos = CTZ32(mask);
				if (!is_new_event_256(ch + pos + 1)) {
					*(ch + pos) = CHANGE_LF;
					char& prev_ch = *(ch + pos - 1);
					if (prev_ch == CR) {
						prev_ch = CHANGE_CR;
					}
				}
				// Сбрасываем обработанный бит
				mask &= ~(1U << pos);
			}
		}
	}

#ifdef __linux__
	__attribute__((target("avx2")))
#endif
	inline void FlatLog::unflat_chank_256(char* ch, size_t size, size_t block_size) {
		__m256i newline_mask = _mm256_set1_epi8(CHANGE_LF);
		char* end = ch + ((size / block_size) - 1) * block_size;

		for (; ch < end; ch += block_size) {
			__m256i block = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ch));
			uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, newline_mask));

			while (mask != 0) {
				// Находим позицию первого установленного бита
				size_t pos = CTZ32(mask);
				*(ch + pos) = LF;
				char& prev_ch = *(ch + pos - 1);
				if (prev_ch == CHANGE_CR) {
					prev_ch = CR;
				}
				// Сбрасываем обработанный бит
				mask &= ~(1U << pos);
			}
		}
	}

	inline void FlatLog::flat_chank_none(char* ch, size_t size, size_t block_size) {
		char* end = ch + ((size / block_size) - 1) * block_size;
		for (; ch < end; ++ch) {
			if (*ch == LF && !is_new_event(ch + 1)) {
				*(ch) = CHANGE_LF;
				if (*(ch - 1) == CR) {
					*(ch - 1) = CHANGE_CR;
				}
			}
		}
	}

	inline void FlatLog::unflat_chank_none(char* ch, size_t size, size_t block_size) {
		char* end = ch + ((size / block_size) - 1) * block_size;
		for (; ch < end; ++ch) {
			if (*ch == CHANGE_LF) {
				*(ch) = LF;
				if (*(ch - 1) == CHANGE_CR) {
					*(ch - 1) = CR;
				}
			}
		}
	}

#ifdef __linux__
	__attribute__((target("avx512f,avx512bw")))
#endif
	inline bool FlatLog::is_new_event_512(char* ch) {
		//19:00.501005 - признак нового события 12 символов
		static const __m512i zero = _mm512_set1_epi8('0');
		static const __m512i nine = _mm512_set1_epi8('9');
		static const __m512i colon = _mm512_set1_epi8(':');
		static const __m512i dot = _mm512_set1_epi8('.');
		static const __mmask64 pattern_mask =
			(1ULL << 0) | (1ULL << 1) |   // \d\d
			(1ULL << 3) | (1ULL << 4) |   // :\d\d  
			(1ULL << 6) | (1ULL << 7) | (1ULL << 8) |
			(1ULL << 9) | (1ULL << 10) | (1ULL << 11);  // \.\d{6}

		__m512i data = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ch));
		__mmask64 is_digit_mask = _mm512_cmple_epu8_mask(data, nine) &
			_mm512_cmpge_epu8_mask(data, zero);

		if ((is_digit_mask & pattern_mask) != pattern_mask) {
			return false;
		}

		__mmask64 colon_mask = _mm512_cmpeq_epi8_mask(data, colon);
		__mmask64 dot_mask = _mm512_cmpeq_epi8_mask(data, dot);

		return (colon_mask & (1ULL << 2)) && (dot_mask & (1ULL << 5));
	}

#ifdef __linux__
	__attribute__((target("avx2")))
#endif
	inline bool FlatLog::is_new_event_256(char* ch) {
		//19:00.501005 - признак нового события 12 символов
		static const __m256i zero = _mm256_set1_epi8('0');
		static const __m256i nine = _mm256_set1_epi8('9');
		static const __m256i colon = _mm256_set1_epi8(':');
		static const __m256i dot = _mm256_set1_epi8('.');
		static const uint32_t pattern_mask =
			(1U << 0) | (1U << 1) |   // \d\d
			(1U << 3) | (1U << 4) |   // :\d\d  
			(1U << 6) | (1U << 7) | (1U << 8) |
			(1U << 9) | (1U << 10) | (1U << 11);  // \.\d{6}

		__m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ch));

		// Проверка цифр: data >= '0' && data <= '9'
		__m256i ge_zero = _mm256_cmpeq_epi8(_mm256_max_epu8(data, zero), data); // data >= '0'
		__m256i le_nine = _mm256_cmpeq_epi8(_mm256_min_epu8(data, nine), data); // data <= '9'
		__m256i is_digit = _mm256_and_si256(ge_zero, le_nine);
		uint32_t is_digit_mask = _mm256_movemask_epi8(is_digit);

		if ((is_digit_mask & pattern_mask) != pattern_mask) {
			return false;
		}

		// Проверка двоеточия и точки
		__m256i is_colon = _mm256_cmpeq_epi8(data, colon);
		__m256i is_dot = _mm256_cmpeq_epi8(data, dot);
		uint32_t colon_mask = _mm256_movemask_epi8(is_colon);
		uint32_t dot_mask = _mm256_movemask_epi8(is_dot);

		return (colon_mask & (1U << 2)) && (dot_mask & (1U << 5));
	}
	
	inline bool FlatLog::is_new_event(char* ch) {
		//19:00.501005 - признак нового события 12 символов
		return
			*ch >= '0' && *ch <= '9'
			&& *(ch + 1) >= '0' && *(ch + 1) <= '9'
			&& *(ch + 2) == ':'
			&& *(ch + 3) >= '0' && *(ch + 3) <= '9'
			&& *(ch + 4) >= '0' && *(ch + 4) <= '9'
			&& *(ch + 5) == '.'
			&& *(ch + 6) >= '0' && *(ch + 6) <= '9'
			&& *(ch + 7) >= '0' && *(ch + 7) <= '9'
			&& *(ch + 8) >= '0' && *(ch + 8) <= '9'
			&& *(ch + 9) >= '0' && *(ch + 9) <= '9'
			&& *(ch + 10) >= '0' && *(ch + 10) <= '9'
			&& *(ch + 11) >= '0' && *(ch + 11) <= '9';
	}

	void FlatLog::flat_remainder(char* ch, size_t size) {
		//19:00.501005 - 12 символов
		static const size_t lenght_is_new_line = 12;

		char* end = ch + size - lenght_is_new_line;
		for (; ch < end; ++ch) {
			if (*ch == LF && !is_new_event(ch + 1)) {
				*(ch) = CHANGE_LF;
				if (*(ch - 1) == CR) {
					*(ch - 1) = CHANGE_CR;
				}
			}
		}
	}
	
}