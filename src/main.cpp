#include <iostream>
#include <spdlog/spdlog.h>

#include <winsock2.h>

#include "server.hpp"
#include "utils.hpp"

#include <spdlog/sinks/stdout_color_sinks-inl.h>

int main()
{
	spdlog::set_default_logger(spdlog::stdout_color_mt("server"));
	WSADATA wsa_data;

	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
	{
		log_and_throw(std::format("WSAStartup failed. Reason : ",
								  utils_lib::get_last_error_as_str()));
	}

	if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2)
	{
		log_and_throw("Version 2.2 of Winsock not available.");
	}

	http_lib::spawn_servers();
}