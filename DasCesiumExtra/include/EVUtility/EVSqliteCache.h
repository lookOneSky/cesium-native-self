#pragma once
#include <string>
#include <memory>
#include <stdexcept>
#include <optional>
#include <vector>
#include "spdlog/formatter.h"
#include "spdlog/logger.h"
#include <gsl/span>

namespace CesiumAsync {

	template <typename... Args>
	std::string string_format(const std::string& format, Args... args) {
		// extra space for \0
		uint32_t size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
		if (size_s <= 0) {
			throw std::runtime_error("Error during formatting.");
		}
		auto size = static_cast<size_t>(size_s);
		auto buf = std::make_unique<char[]>(size);
		std::snprintf(buf.get(), size, format.c_str(), args...);
		// we don't want the \0 here
		return std::string(buf.get(), buf.get() + size - 1);
	}

	/// <summary>
	/// 旧版sqliteCache中添加的内容移到这里
	/// </summary>
	class EVSqliteCache
	{
	public:
		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		EVSqliteCache();
		~EVSqliteCache();

		/// <summary>
		/// retrive data from db.files
		/// </summary>
		/// <param name="pLogger"></param>
		/// <param name="base_path"></param>
		/// <param name="level"></param>
		/// <param name="row"></param>
		/// <param name="column"></param>
		/// <param name="isHeightMap"></param>
		/// <param name="isVector"></param>
		/// <returns></returns>
		static std::optional<std::vector<std::byte>> readTile(
			const std::shared_ptr<spdlog::logger>& pLogger,
			const std::string& base_path,
			uint32_t level,
			uint32_t row,
			uint32_t column,
			bool isHeightMap,
			bool isVector);

		/// <summary>
		/// 静态方法可以移到外部
		/// </summary>
		/// <param name="base_path"></param>
		/// <param name="level"></param>
		/// <param name="row"></param>
		/// <param name="col"></param>
		/// <param name="isHeightMap"></param>
		/// <param name="isVector"></param>
		/// <returns></returns>
		static bool calculateDataBasePath(
			std::string& base_path,
			uint32_t level,
			uint32_t row,
			uint32_t col,
			bool isHeightMap,
			bool isVector);
		/// <summary>
		/// 解压
		/// </summary>
		/// <param name="compressedBytes"></param>
		/// <param name="uncompressedBytes"></param>
		/// <returns></returns>
		static bool gzipInflate(
                    const gsl::span<const std::byte>& compressedBytes,
                    std::vector<std::byte>& uncompressedBytes);
		/// <summary>
		/// 压缩为gz文件
		/// </summary>
		/// <param name="input"></param>
		/// <param name="inPutSize"></param>
		/// <param name="output_file"></param>
		/// <returns></returns>
		static bool compress_to_gzip(
                    const char* input,
                    int inPutSize,
                    const std::string& output_file);
		/// <summary>
		/// 向gz文件中添加流
		/// </summary>
		/// <param name="input"></param>
		/// <param name="inPutSize"></param>
		/// <param name="output_file"></param>
		/// <returns></returns>
		static bool append_to_gzip(
                    const char* input,
                    int inPutSize,
                    const std::string& output_file);
	private:

	};
}
