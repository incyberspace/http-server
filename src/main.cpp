#include <iostream>
#include <spdlog/spdlog.h>

#include <winsock2.h>

#include "server.hpp"
#include "utils.hpp"

int main()
{
	spdlog::set_level(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::debug);

	WSADATA wsa_data;

	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
	{
		log_and_throw(std::format("WSAStartup failed. Reason : {}",
								  utils_lib::get_last_error_as_str()));
	}

	if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2)
	{
		log_and_throw("Version 2.2 of Winsock is not available.");
	}

	http_lib::spawn_servers();
}