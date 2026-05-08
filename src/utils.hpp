#pragma once

#include <filesystem>
#include <spdlog/spdlog.h>

#define path_to_c_str(path)                                                    \
	(reinterpret_cast<const char *>((path).u8string().c_str()))

#define log_and_throw(msg)                                                     \
	do                                                                         \
	{                                                                          \
		std::string dup_msg = msg;                                             \
		SPDLOG_ERROR((dup_msg));                                               \
		throw std::runtime_error((dup_msg));                                   \
	} while (false)

namespace utils_lib
{
	void strip(std::string &s);

	[[nodiscard]] std::wstring get_last_error_as_wstr();
	[[nodiscard]] std::string get_last_error_as_str();
	[[nodiscard]] std::string wstr_to_str(const std::wstring &wstr);
} // namespace utils_lib
