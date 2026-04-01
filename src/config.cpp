#include "config.hpp"

#include <algorithm>
#include <fstream>
#include <ranges>

#include "utils.hpp"

#include <spdlog/spdlog.h>

namespace config_lib
{
	ConfigProxy::ConfigProxy(const ValueVariant &value) : value_(value)
	{
	}

	Config::Config(const std::filesystem::path &config_path)
	{
		std::ifstream config_file(config_path);

		if (!config_file.is_open())
		{
			log_and_throw(std::format("Failed to open the config file : {}",
									  path_to_c_str(config_path)));
		}

		std::string line;
		while (std::getline(config_file, line))
		{
			if (line.empty())
			{
				continue;
			}

			const auto delim_it =
				std::ranges::find(line.begin(), line.end(), '=');
			if (delim_it == line.begin() ||
				delim_it == std::prev(line.end() - 1) || delim_it == line.end())
			{
				log_and_throw(std::format(
					"Failed to parse the config file. Invalid line : {}",
					line));
			}

			const int delim_pos =
				static_cast<int>(std::distance(line.begin(), delim_it));

			std::string key = line.substr(0, delim_pos);
			utils_lib::strip(key);

			std::string value = line.substr(delim_pos + 1);
			utils_lib::strip(value);

			ConfigProxy::ValueVariant value_variant;

			std::string value_in_upper = value |
										 std::views::transform(toupper) |
										 std::ranges::to<std::string>();
			if (value_in_upper == "TRUE" || value_in_upper == "ON")
			{
				value_variant = true;
			}
			else if (value_in_upper == "FALSE" || value_in_upper == "OFF")
			{
				value_variant = false;
			}
			else
			{
				bool is_integer = true;
				int integer;

				try
				{
					integer = std::stoi(value);
				}
				catch ([[maybe_unused]] const std::exception &e)
				{
					is_integer = false;
				}

				if (std::ranges::count_if(value, [](const char ch) -> bool {
						return !isdigit(ch);
					}) > 1)
				{
					is_integer = false;
				}

				if (is_integer)
				{
					value_variant = integer;
				}
				else
				{
					value_variant = value;
				}
			}

			data_[key | std::views::transform(toupper) |
				  std::ranges::to<std::string>()] = value_variant;
		}
	}

	ConfigProxy Config::get(const std::string &key) const
	{
		if (!data_.contains(key))
		{
			log_and_throw(std::format("The key : {} doesn't exist", key));
		}

		return ConfigProxy(data_.at(key));
	}

	std::optional<ConfigProxy> Config::try_get(const std::string &key) const
	{
		if (!data_.contains(key))
		{
			return std::nullopt;
		}

		return ConfigProxy(data_.at(key));
	}
} // namespace config_lib