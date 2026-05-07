#include <spdlog/spdlog.h>

#include "server.hpp"
#include "socket.hpp"

int main()
{
	spdlog::set_level(spdlog::level::off);
	spdlog::flush_on(spdlog::level::debug);

	socket_lib::init_socket();
	http_lib::Server::start_spawning_servers("./config.conf");
	http_lib::Server::join_server_spawner();
}
