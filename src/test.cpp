#include <cstdio>
#include <filesystem>
#include <fstream>

#include "config.hpp"

#include <gtest/gtest.h>

namespace test_lib
{
	TEST(config_test, read)
	{
		std::unique_ptr<const char[]> tmp_config_path_c_str;
		tmp_config_path_c_str.reset(std::tmpnam(nullptr));
		std::filesystem::path tmp_config_path = tmp_config_path_c_str.get() + 1;

		{
			std::ofstream config_file(tmp_config_path);

			config_file << "T1=TrUE" << std::endl;
			config_file << " T2  =oN" << std::endl;
			config_file << "F1= FalsE " << std::endl;
			config_file << "F2 =oFf " << std::endl;

			config_file << "STR_KEY= STR " << std::endl;
			config_file << " INT_KEY =10" << std::endl;
		}

		config_lib::Config config(tmp_config_path);

		ASSERT_EQ(static_cast<bool>(config.get("T1")), true);
		ASSERT_EQ(static_cast<bool>(config.get("T2")), true);
		ASSERT_EQ(static_cast<bool>(config.get("F1")), false);
		ASSERT_EQ(static_cast<bool>(config.get("F2")), false);

		ASSERT_EQ(static_cast<std::string>(config.get("STR_KEY")), "STR");
		ASSERT_EQ(static_cast<int>(config.get("INT_KEY")), 10);

		std::filesystem::remove(tmp_config_path);
	}
} // namespace test_lib