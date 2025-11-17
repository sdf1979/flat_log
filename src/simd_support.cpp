#include "simd_support.h"

namespace soldy {

	SimdSupport::SimdLevel SimdSupport::BestLevel() {
		detect();

		if (caps_.avx512_f) return SimdLevel::AVX512;
		if (caps_.avx2) return SimdLevel::AVX2;
		if (caps_.avx) return SimdLevel::AVX;
		if (caps_.sse4_2) return SimdLevel::SSE4_2;
		if (caps_.sse4_1) return SimdLevel::SSE4_1;
		if (caps_.ssse3) return SimdLevel::SSSE3;
		if (caps_.sse3) return SimdLevel::SSE3;
		if (caps_.sse2) return SimdLevel::SSE2;
		if (caps_.sse) return SimdLevel::SSE;

		return SimdLevel::None;
	}

	size_t SimdSupport::BlockSize(SimdLevel simd_level) {
		detect();

		if (simd_level == SimdLevel::AVX512) return 64;
		if (simd_level == SimdLevel::AVX2) return 32;
		if (simd_level == SimdLevel::AVX) return 32;
		if (simd_level == SimdLevel::SSE4_2) return 16;
		if (simd_level == SimdLevel::SSE4_1) return 16;
		if (simd_level == SimdLevel::SSSE3) return 16;
		if (simd_level == SimdLevel::SSE3) return 16;
		if (simd_level == SimdLevel::SSE2) return 16;
		if (simd_level == SimdLevel::SSE) return 16;

		return 0;
	}

	SimdSupport::SimdLevel SimdSupport::StringToSimdLevel(const std::wstring& simd_level) {

		if (simd_level == L"avx512") return SimdLevel::AVX512;
		if (simd_level == L"avx2") return SimdLevel::AVX2;
		if (simd_level == L"avx") return SimdLevel::AVX;
		if (simd_level == L"sse4_2") return SimdLevel::SSE4_2;
		if (simd_level == L"sse4_1") return SimdLevel::SSE4_1;
		if (simd_level == L"ssse3") return SimdLevel::SSSE3;
		if (simd_level == L"sse3") return SimdLevel::SSE3;
		if (simd_level == L"sse2") return SimdLevel::SSE2;
		if (simd_level == L"sse") return SimdLevel::SSE;

		return SimdLevel::None;
	}

	std::wstring SimdSupport::SimdLevelToString(SimdSupport::SimdLevel simd_level) {
		if (simd_level == SimdLevel::AVX512) return L"avx512";
		if (simd_level == SimdLevel::AVX2) return L"avx2";
		if (simd_level == SimdLevel::AVX) return L"avx";
		if (simd_level == SimdLevel::SSE4_2) return L"sse4_2";
		if (simd_level == SimdLevel::SSE4_1) return L"sse4_1";
		if (simd_level == SimdLevel::SSSE3) return L"ssse3";
		if (simd_level == SimdLevel::SSE3) return L"sse3";
		if (simd_level == SimdLevel::SSE2) return L"sse2";
		if (simd_level == SimdLevel::SSE) return L"sse";

		return L"none";
	}

	std::wstring SimdSupport::ToString() {
		detect();
		std::wstring str;
		str.append(L"YOU CPU SIMD Support:\n  AVX512=").append(caps_.avx512_f ? L"Y, " : L"N, ")
			.append(L"AVX512BW=").append(caps_.avx512_bw ? L"Y, " : L"N, ")
			.append(L"AVX512DQ=").append(caps_.avx512_dq ? L"Y, " : L"N, ")
			.append(L"AVX512VL=").append(caps_.avx512_vl ? L"Y\n  " : L"N\n  ")
			.append(L"AVX2=").append(caps_.avx2 ? L"Y, " : L"N, ")
			.append(L"AVX=").append(caps_.avx ? L"Y\n  " : L"N\n  ")
			.append(L"SE4.2=").append(caps_.sse4_2 ? L"Y, " : L"N, ")
			.append(L"SSE4.1=").append(caps_.sse4_1 ? L"Y, " : L"N, ")
			.append(L"SSSE3=").append(caps_.ssse3 ? L"Y, " : L"N, ")
			.append(L"SSE3=").append(caps_.sse3 ? L"Y, " : L"N, ")
			.append(L"SSE2=").append(caps_.sse2 ? L"Y, " : L"N, ")
			.append(L"SSE=").append(caps_.sse ? L"Y" : L"N");
		return str;
	}
    
	void SimdSupport::cpu_id(int regs[4], int leaf, int subleaf) {
#if defined(_MSC_VER)
		__cpuidex(regs, leaf, subleaf);
#else
		__asm__ __volatile__(
			"cpuid"
			: "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
			: "a"(leaf), "c"(subleaf)
		);
#endif
	}

	void SimdSupport::detect() {
		if (!is_detect_) {
			is_detect_ = true;
			int regs[4];

			// Проверка стандартных возможностей (Leaf 1)
			cpu_id(regs, 1);
			caps_.sse = (regs[3] & (1 << 25)) != 0;  // EDX bit 25: SSE
			caps_.sse2 = (regs[3] & (1 << 26)) != 0;  // EDX bit 26: SSE2
			caps_.sse3 = (regs[2] & (1 << 0)) != 0;  // ECX bit 0:  SSE3
			caps_.ssse3 = (regs[2] & (1 << 9)) != 0;  // ECX bit 9:  SSSE3
			caps_.sse4_1 = (regs[2] & (1 << 19)) != 0;  // ECX bit 19: SSE4.1
			caps_.sse4_2 = (regs[2] & (1 << 20)) != 0;  // ECX bit 20: SSE4.2
			caps_.avx = (regs[2] & (1 << 28)) != 0;  // ECX bit 28: AVX

			// Проверка расширенных возможностей (Leaf 7, Subleaf 0)
			cpu_id(regs, 7, 0);
			caps_.avx2 = (regs[1] & (1 << 5)) != 0;  // EBX bit 5:  AVX2
			caps_.avx512_f = (regs[1] & (1 << 16)) != 0;  // EBX bit 16: AVX512F
			caps_.avx512_cd = (regs[1] & (1 << 28)) != 0;  // EBX bit 28: AVX512CD
			caps_.avx512_bw = (regs[1] & (1 << 30)) != 0;  // EBX bit 30: AVX512BW
			caps_.avx512_dq = (regs[1] & (1 << 17)) != 0;  // EBX bit 17: AVX512DQ
			caps_.avx512_vl = (regs[1] & (1 << 31)) != 0;  // EBX bit 31: AVX512VL
		}
	}

}