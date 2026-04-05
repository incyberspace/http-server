#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <winsock2.h>

#include "socket.hpp"

namespace http_lib
{
	struct Method
	{
		std::string type;
		std::string target;
		std::vector<std::string> queries;
		std::string http_version;
	};

	struct Request
	{
		Method method;
		std::unordered_map<std::string, std::string> headers;
		std::vector<char> body;
	};

	void spawn_servers();

	class Server final
	{
	  public:
		explicit Server(
			socket_lib::SockWrapper sock,
			int sock_stream_buf_size = default_sock_stream_buf_size);

		void run();

	  private:
		int sock_stream_buf_size_;
		static constexpr int default_sock_stream_buf_size = 100;
		socket_lib::SockStream sock_stream_;
		[[nodiscard]] Request get_request();
		void send_response(const Request &request) const;
	};
} // namespace http_lib