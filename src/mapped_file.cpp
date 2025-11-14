#include "mapped_file.h"

namespace soldy {

	bool MappedFile::OpenSequential(const std::filesystem::path& file_path, std::error_code& ec) {
		close();

		if (!std::filesystem::exists(file_path, ec)) {
			if (!ec) ec = std::make_error_code(std::errc::no_such_file_or_directory);
			return false;
		}

		file_size_ = std::filesystem::file_size(file_path, ec);
		if (ec) return false;

#ifdef _WIN32
		//file_handle_ = CreateFileW(file_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		file_handle_ = CreateFileW(file_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

		if (file_handle_ == INVALID_HANDLE_VALUE) {
			ec = std::error_code( GetLastError(), std::system_category());
			file_handle_ = nullptr;
			file_size_ = 0;
			return false;
		}

		file_mapping_handle_ = CreateFileMapping(file_handle_, nullptr, PAGE_READWRITE, 0, 0, nullptr);
		if (!file_mapping_handle_) {
			ec = std::error_code(GetLastError(), std::system_category());
			CloseHandle(file_handle_);
			file_handle_ = nullptr;
			file_size_ = 0;
			return false;
		}

		if (!page_size_) {
			SYSTEM_INFO sys_info;
			GetSystemInfo(&sys_info);
			page_size_ = sys_info.dwAllocationGranularity;
		}
#else
		fd_ = ::open(file_path.c_str(), O_RDWR);
		if (fd_ == -1) {
			ec = std::error_code(errno, std::system_category());
			return false;
		}
		if (!page_size_) {
			page_size_ = sysconf(_SC_PAGE_SIZE);
		}
#endif

		return true;
	}

	bool MappedFile::MapRegion(size_t offset, size_t size, std::error_code& ec) {
		unmap_current_region();
		if (offset >= file_size_ || offset + size > file_size_ || !size) {
			ec = std::make_error_code(std::errc::invalid_argument);
			return false;
		}

		// Выравниваем offset вниз до границы page
		size_t aligned_offset = offset & ~(page_size_ - 1);
		cur_mapping_offset_delta_ = offset - aligned_offset;
		offset -= cur_mapping_offset_delta_;

#ifdef _WIN32
		cur_mapping_ = MapViewOfFile(
			file_mapping_handle_,
			FILE_MAP_WRITE,
			static_cast<DWORD>(offset >> 32),
			static_cast<DWORD>(offset & 0xFFFFFFFF),
			size + cur_mapping_offset_delta_
		);
		if (!cur_mapping_) {
			cur_mapping_offset_delta_ = 0;
			cur_mapping_size_ = 0;
			ec = std::error_code(GetLastError(), std::system_category());
			return false;
		}
#else
		cur_mapping_ = mmap(nullptr, size + cur_mapping_offset_delta_, PROT_WRITE, MAP_SHARED, fd_, offset);
		if (cur_mapping_ == MAP_FAILED) {
			cur_mapping_offset_delta_ = 0;
			cur_mapping_size_ = 0;
			ec = std::error_code(errno, std::system_category());
			return false;
		}
#endif
		cur_mapping_ = static_cast<char*>(cur_mapping_) + cur_mapping_offset_delta_;
		cur_mapping_size_ = size;
		return true;
	}

	void MappedFile::unmap_current_region() {
		if (cur_mapping_) {
#ifdef _WIN32
			FlushViewOfFile(static_cast<char*>(cur_mapping_) - cur_mapping_offset_delta_, cur_mapping_size_ + cur_mapping_offset_delta_);
			UnmapViewOfFile(static_cast<char*>(cur_mapping_) - cur_mapping_offset_delta_);
#else
			msync(static_cast<char*>(cur_mapping_) - cur_mapping_offset_delta_, cur_mapping_size_ + cur_mapping_offset_delta_, MS_SYNC);
			munmap(static_cast<char*>(cur_mapping_) - cur_mapping_offset_delta_, cur_mapping_size_ + cur_mapping_offset_delta_);
#endif
			cur_mapping_ = nullptr;
			cur_mapping_size_ = 0;
			cur_mapping_offset_delta_ = 0;
		}
	}

	void MappedFile::close() {
		unmap_current_region();
#ifdef _WIN32
		if (file_mapping_handle_) {
			CloseHandle(file_mapping_handle_);
			file_mapping_handle_ = nullptr;
		}
		if (file_handle_) {
			CloseHandle(file_handle_);
			file_handle_ = nullptr;
		}
#else
		if (fd_ != -1) {
			::close(fd_);
			fd_ = -1;
		}
#endif
		file_size_ = 0;
	}
}