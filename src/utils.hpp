#pragma once

#include <filesystem>
#include <spdlog/spdlog.h>

#define path_to_c_str(path)                                                    \
	(reinterpret_cast<const char *>((path).string().c_str()))

#define log_and_throw(msg)                                                     \
	spdlog::error((msg)), throw std::runtime_error((msg));

namespace utils_lib
{
	void strip(std::string &s);
} // namespace utils_lib
