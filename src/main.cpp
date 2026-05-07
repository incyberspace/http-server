#include <spdlog/spdlog.h>

#include "server.hpp"
#include "socket.hpp"

int main()
{
	spdlog::set_level(spdlog::level::off);
	//spdlog::flush_on(spdlog::level::debug);

    socket_lib::init_socket();
	http_lib::start_spawning_servers();
}
