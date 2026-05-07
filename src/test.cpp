#include <filesystem>
#include <fstream>
#include <thread>

#include "config.hpp"
#include "server.hpp"
#include "socket.hpp"
#include "spdlog/common.h"
#include "utils.hpp"

#include <HTTPRequest.hpp>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

namespace test_lib
{
    const std::string &get_domain()
    {
        static const std::string domain =
            "http://localhost:" + std::to_string(static_cast<int>(
                               config_lib::config.get("PORT")));
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

    struct InitSocketHelper
    {
        InitSocketHelper()
        {
            socket_lib::init_socket();
        }
    } init_socket_helper;

    TEST(server_test, valid_requests)
    {
        http_lib::start_spawning_servers();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        try
        {
            http::Request request{get_domain()};

            const auto response = request.send("GET");
            std::cout << std::string{response.body.begin(),
                                     response.body.end()}
                      << '\n';
        }
        catch (const std::exception &e)
        {
            std::cerr << "Request failed, error: " << e.what() << '\n';
        }

        http_lib::stop_spawning_servers();
    }
} // namespace test_lib
