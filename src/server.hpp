#pragma once

#include <boost/asio/ip/tcp.hpp>

using boost::asio::ip::tcp;

namespace http_lib
{
	class Server final
	{
	public:
		explicit Server(const tcp &socket);
		Server(Server const& other) = delete;
	private:
		tcp socket_;
	};
}