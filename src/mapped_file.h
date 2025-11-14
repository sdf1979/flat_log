#pragma once

#include <filesystem>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace soldy {

	class MappedFile {
	private:
#ifdef _WIN32
		HANDLE file_handle_ = nullptr;
		HANDLE file_mapping_handle_ = nullptr;
#else
		int fd_ = -1;
#endif
		void* cur_mapping_ = nullptr;
		size_t cur_mapping_size_ = 0;
		size_t cur_mapping_offset_delta_ = 0;
		size_t file_size_ = 0;
		size_t page_size_ = 0;
		void unmap_current_region();
		void close();
	public:
		MappedFile() = default;
		~MappedFile() { close(); }
		MappedFile(const MappedFile&) = delete;
		MappedFile& operator=(const MappedFile&) = delete;
		MappedFile(MappedFile&& other) = delete;
		MappedFile& operator=(MappedFile&& other) = delete;

		bool OpenSequential(const std::filesystem::path& file_path, std::error_code& ec);
		bool MapRegion(size_t offset, size_t size, std::error_code& ec);
		void* Data() const noexcept { return cur_mapping_; }
		size_t MapSize() const noexcept { return cur_mapping_size_; }
		size_t FileSize() const noexcept { return file_size_; }
	};

}