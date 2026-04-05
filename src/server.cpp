#include "server.hpp"

#include <regex>
#include <thread>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "config.hpp"

namespace
{
} // namespace

namespace http_lib
{
	void spawn_servers()
	{
		const socket_lib::SockWrapper listen_sock =
			socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listen_sock.get() == INVALID_SOCKET)
		{
			log_and_throw(
				std::format("Could not create listening socket. Reason : {}",
							utils_lib::get_last_error_as_str()));
		}

		sockaddr_in addr_info;
		addr_info.sin_family = AF_INET;
		addr_info.sin_port =
			htons(static_cast<int>(config_lib::config.get("PORT")));
		inet_pton(
			AF_INET,
			static_cast<std::string>(config_lib::config.get("LISTEN")).c_str(),
			&addr_info.sin_addr);

		if (bind(listen_sock.get(), reinterpret_cast<sockaddr *>(&addr_info),
				 sizeof addr_info) == SOCKET_ERROR)
		{
			log_and_throw(std::format("Bind failed. Reason : {}",
									  utils_lib::get_last_error_as_str()));
		}

		if (listen(listen_sock.get(), 1) == SOCKET_ERROR)
		{
			log_and_throw(std::format("Listen failed. Reason : {}",
									  utils_lib::get_last_error_as_str()));
		}

		std::vector<std::jthread> server_workers;
		while (true)
		{
			socket_lib::SockWrapper accept_sock =
				accept(listen_sock.get(), nullptr, nullptr);

			if (accept_sock.get() == INVALID_SOCKET)
			{
				spdlog::error("Accept failed. Reason : {}",
							  utils_lib::get_last_error_as_str());
				break;
			}

			u_long mode = 1;
			if (ioctlsocket(accept_sock.get(), FIONBIO, &mode) != 0)
			{
				spdlog::error("ioctlsocket failed. Reason : {}",
							  utils_lib::get_last_error_as_str());
				break;
			}

			spdlog::info("Accepted");
			server_workers.emplace_back(
				[accept_sock = std::move(accept_sock)]() mutable -> void {
					Server server(std::move(accept_sock));
					server.run();
				});
		}
	}

	Server::Server(socket_lib::SockWrapper sock, const int sock_stream_buf_size)
		: sock_stream_buf_size_(sock_stream_buf_size),
		  sock_stream_(std::move(sock), sock_stream_buf_size)
	{
	}

	Request Server::get_request()
	{
		Request result;

		std::string line;
		line = sock_stream_.read_line();

		const std::regex re{R"((\S+) (\S+?)(?:\?(\S+))? HTTP\/(\S+))"};
		std::smatch tokens;
		std::regex_search(line, tokens, re);

		if (tokens.size() != 5)
		{
			spdlog::warn("An invalid request line {}", line);
			return result;
		}

		Method method;
		method.type = tokens[1];
		method.target = tokens[2];

		std::vector<std::string> queries;
		std::stringstream ss(tokens[3]);

		std::string query;
		while (getline(ss, query, '&'))
		{
			queries.push_back(query);
		}

		method.queries = std::move(queries);
		method.http_version = tokens[4];

		result.method = std::move(method);

		while (true)
		{
			line = sock_stream_.read_line();

			if (line.empty())
			{
				break;
			}

			const auto space_it = line.find(' ');
			result.headers[line.substr(0, space_it - 1) |
						   std::views::transform([](const char ch) -> char {
							   return static_cast<char>(tolower(ch));
						   }) |
						   std::ranges::to<std::string>()] =
				line.substr(space_it + 1);
		}

		if (result.headers.contains("content-length"))
		{
			result.body = sock_stream_.read_all(
				std::stoi(result.headers.at("content-length")));
		}

		return result;
	}

	void Server::send_response(const Request &request) const
	{
		using namespace std::string_literals;

		sock_stream_.send_all(
			"HTTP/1.1 200 OK\r\n"s); // Raw strings include NUL thus don't work
		sock_stream_.send_all("Content-Length: 0\r\n"s);
		sock_stream_.send_all("\r\n"s);
	}

	void Server::run()
	{
		while (true)
		{
			Request request = get_request();
			send_response(request);
		}
	}
} // namespace http_lib