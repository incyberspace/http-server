#include "utils.hpp"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace utils_lib
{
	void strip(std::string &s)
	{
		std::string stripped;
		const auto s_copy_begin =
			std::ranges::find_if(s.begin(), s.end(), [](const char ch) -> bool {
				return !isspace(ch);
			});
		const auto s_copy_end = std::ranges::find_if(s.rbegin(), s.rend(),
													 [](const char ch) -> bool {
														 return !isspace(ch);
													 })
									.base();

		std::for_each(
			s_copy_begin, s_copy_end,
			[&stripped](const char ch) -> void { stripped.push_back(ch); });
		s = std::move(stripped);
	}
} // namespace utils_lib