#include <filesystem>
#include <fstream>
#include <thread>

#include "config.hpp"
#include "server.hpp"
#include "socket.hpp"
#include "spdlog/common.h"
#include "utils.hpp"

#include <gtest/gtest.h>
#include <httplib.h>
#include <spdlog/spdlog.h>

namespace test_lib
{
	const config_lib::Config &get_test_config()
	{
		static const config_lib::Config config("./test/config.conf");
		return config;
	}

	const std::string &get_domain()
	{
		static const std::string domain =
			"http://localhost:" +
			std::to_string(static_cast<int>(get_test_config().get("PORT")));
		return domain;
	}

	struct LoggerInitHelper
	{
		LoggerInitHelper()
		{
			spdlog::set_level(spdlog::level::debug);
		}
	} logger_init_helper;

	TEST(config_test, read)
	{
		const char *const tmp_config_path_c_str = std::tmpnam(nullptr);
		std::filesystem::path tmp_config_path;

		spdlog::debug("c_str : {}", tmp_config_path_c_str);

		{
			std::ofstream config_file;

			if (tmp_config_path_c_str[0] == '\\')
			{
				tmp_config_path = tmp_config_path_c_str + 1;
			}
			else
			{
				tmp_config_path = tmp_config_path_c_str;
			}

			config_file.open(tmp_config_path);

			config_file << "T1=TrUE" << std::endl;
			config_file << " T2  =oN" << std::endl;
			config_file << "F1= FalsE " << std::endl;
			config_file << "F2 =oFf " << std::endl;

			config_file << "STR_KEY= STR " << std::endl;
			config_file << " INT_KEY =10" << std::endl;
		}

		spdlog::debug("config_path : {}", path_to_c_str(tmp_config_path));
		config_lib::Config config(tmp_config_path);

		ASSERT_EQ(static_cast<bool>(config.get("T1")), true);
		ASSERT_EQ(static_cast<bool>(config.get("T2")), true);
		ASSERT_EQ(static_cast<bool>(config.get("F1")), false);
		ASSERT_EQ(static_cast<bool>(config.get("F2")), false);

		ASSERT_EQ(static_cast<std::string>(config.get("STR_KEY")), "STR");
		ASSERT_EQ(static_cast<int>(config.get("INT_KEY")), 10);

		std::filesystem::remove(tmp_config_path);
	}

	class server_test : public ::testing::Test
	{
	  protected:
		static void SetUpTestCase()
		{
			socket_lib::init_socket();
			http_lib::Server::start_spawning_servers(get_test_config());
		}

		static void TearDownTestCase()
		{
			http_lib::Server::stop_spawning_servers();
			async_lib::EventLoopManager::get_instance().end_event_loop();
		}
	};

	TEST_F(server_test, valid_requests)
	{
		httplib::Client cli(
			"127.0.0.1",
			get_test_config().get("PORT")); // localhost doesn't seem to work

		auto res = cli.Get("/");
		ASSERT_TRUE(res);

		std::ifstream index_html("./test/index.html");
		index_html.seekg(0, std::ios::end);
		const int index_html_size = index_html.tellg();
		index_html.close();

		ASSERT_EQ(res->status, 200);
		ASSERT_EQ(res->body.size(), index_html_size);

		res = cli.Get("/test");
		ASSERT_TRUE(res);
		ASSERT_EQ(res->status, 200);
		ASSERT_EQ(res->body.size(), index_html_size);
	}

	TEST_F(server_test, invalid_requests)
	{
		httplib::Client cli("127.0.0.1", get_test_config().get("PORT"));

		auto res = cli.Get("/unknown");
		ASSERT_TRUE(res);
		ASSERT_EQ(res->status, 404);
		ASSERT_EQ(res->body.size(), 0);

		res = cli.Get("/../");
		ASSERT_TRUE(res);
		ASSERT_EQ(res->status, 403);
		ASSERT_EQ(res->body.size(), 0);

		res = cli.Post("/");
		ASSERT_TRUE(res);
		ASSERT_EQ(res->status, 405);
		ASSERT_EQ(res->body.size(), 0);
	}
} // namespace test_lib
