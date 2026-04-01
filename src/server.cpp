#include "server.hpp"

namespace http_lib
{
	Server::Server(const tcp &socket) : socket_(socket)
	{
	}
}