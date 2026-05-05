#pragma once

#include <filesystem>
#include <optional>
#include <variant>

#include "utils.hpp"

namespace config_lib
{
	class ConfigProxy final
	{
		friend class Config;

		template <typename T>
		friend bool operator==(const ConfigProxy &lhs, const T &rhs);

		template <typename T>
		friend bool operator==(const T &lhs, const ConfigProxy &rhs);

		using ValueVariant = std::variant<int, std::string, bool>;

	  public:
		template <typename T> [[nodiscard]] operator T() const;
		explicit ConfigProxy(const ValueVariant &value);

	  private:
		const ValueVariant &value_;
	};

	template <typename T> ConfigProxy::operator T() const
	{
		if (!std::holds_alternative<T>(value_))
		{
			log_and_throw(
				std::format("The requested type {} isn't the type of value",
							typeid(T).name()));
		}

		return std::get<T>(value_);
	}

	class Config final
	{
	  public:
		explicit Config(const std::filesystem::path &config_path);
		[[nodiscard]] ConfigProxy get(const std::string &key) const;
		[[nodiscard]] std::optional<ConfigProxy> try_get(const std::string &key) const;

	  private:
		std::unordered_map<std::string, ConfigProxy::ValueVariant> data_;
	};

	template <typename T>
	[[nodiscard]] bool operator==(const ConfigProxy &lhs, const T &rhs)
	{
		return static_cast<T>(lhs) == rhs;
	}

	template <typename T>
	[[nodiscard]] bool operator==(const T &lhs, const ConfigProxy &rhs)
	{
		return lhs == static_cast<T>(rhs);
	}

	inline std::filesystem::path default_config_path("./config.conf");
	inline const Config config(default_config_path);
} // namespace config_lib