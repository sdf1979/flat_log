#pragma once

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>
#endif
#include <string>
#include <cwctype>


namespace soldy {

	class SimdSupport {
	private:
		struct SimdCapabilities {
			bool sse;
			bool sse2;
			bool sse3;
			bool ssse3;
			bool sse4_1;
			bool sse4_2;
			bool avx;
			bool avx2;
			bool avx512_f;
			bool avx512_cd;
			bool avx512_bw;
			bool avx512_dq;
			bool avx512_vl;
		};
		SimdCapabilities caps_{};
		void cpu_id(int regs[4], int leaf, int subleaf = 0);
		bool is_detect_ = false;
		void detect();
	public:
		constexpr SimdSupport() = default;
		enum class SimdLevel {
			None = 0,
			SSE,
			SSE2,
			SSE3,
			SSSE3,
			SSE4_1,
			SSE4_2,
			AVX,
			AVX2,
			AVX512
		};
		static SimdLevel StringToSimdLevel(const std::wstring& simd_level);
		static std::wstring SimdLevelToString(SimdLevel simd_level);
		SimdLevel BestLevel();
		size_t BlockSize(SimdLevel simd_level);
		std::wstring ToString();
	};

}