#include "server.hpp"

#include <fstream>
#include <ranges>
#include <regex>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "async.hpp"
#include "config.hpp"

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

		sockaddr_in addr_info = {};
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

		if (listen(listen_sock.get(), SOMAXCONN) == SOCKET_ERROR)
		{
			log_and_throw(std::format("Listen failed. Reason : {}",
									  utils_lib::get_last_error_as_str()));
		}

		std::vector<std::unique_ptr<Server>> spawned_servers;

		do
		{
			spdlog::debug("Server count : {}", spawned_servers.size());
			socket_lib::SockWrapper accept_sock =
				accept(listen_sock.get(), nullptr, nullptr);

			if (accept_sock.get() == INVALID_SOCKET)
			{
				spdlog::error("Accept failed. Reason : {}",
							  utils_lib::get_last_error_as_str());
				break;
			}

			u_long socket_mode = 1;
			if (ioctlsocket(accept_sock.get(), FIONBIO, &socket_mode) != 0)
			{
				spdlog::error("ioctlsocket failed. Reason : {}",
							  utils_lib::get_last_error_as_str());
				break;
			}

			std::erase_if(spawned_servers,
						  [](const std::unique_ptr<Server> &server) -> bool {
							  return server->is_done();
						  });
			spdlog::info("Accepted");
			spawned_servers.emplace_back(
				std::make_unique<Server>(std::move(accept_sock), 100000));
		} while (true);
	}

	Server::Server(socket_lib::SockWrapper sock, const int sock_stream_buf_size)
		: sock_stream_buf_size_(sock_stream_buf_size),
		  sock_stream_(std::move(sock), sock_stream_buf_size),
		  runner_task_(run())
	{
	}

	void Server::resume() const
	{
		runner_task_.resume();
	}

	bool Server::is_done() const
	{
		return runner_task_.is_done();
	}

	async_lib::Task<Request> Server::get_request()
	{
		static const std::regex re{R"((\S+) (\S+?)(?:\?(\S+))? HTTP\/(\S+))"};

		std::string line;
		line = co_await sock_stream_.read_line_async();

		std::smatch tokens;
		std::regex_search(line, tokens, re);

		if (tokens.size() != 5)
		{
			spdlog::warn("An invalid request line {}", line);
			co_return {};
		}

		Method method;
		method.type = tokens[1];
		method.target = tokens[2];

		std::unordered_map<std::string, std::string> queries;
		std::stringstream ss(tokens[3]);

		std::string query;
		while (getline(ss, query, '&'))
		{
			const auto equal_it = query.find('=');
			queries[query.substr(0, equal_it)] = query.substr(equal_it + 1);
		}

		method.queries = std::move(queries);
		method.http_version = tokens[4];

		Request result;
		result.method = std::move(method);

		while (true)
		{
			line = co_await sock_stream_.read_line_async();

			if (line.empty())
			{
				break;
			}

			const auto colon_it = line.find(':');

			std::string key = line.substr(0, colon_it);
			utils_lib::strip(key);
			key = key | std::views::transform([](const char ch) -> char {
					  return static_cast<char>(tolower(ch));
				  }) |
				  std::ranges::to<std::string>();
			std::string value = line.substr(colon_it + 1);
			utils_lib::strip(value);

			result.headers[key] = value;
		}

		if (result.headers.contains("content-length"))
		{
			result.body = co_await sock_stream_.read_all_async(
				std::stoi(result.headers.at("content-length")));
		}

		co_return result;
	}

	async_lib::Task<void> Server::send_response(const Request &request) const
	{
		static constexpr int fstream_buf_size = 1000000;
		using namespace std::string_literals;

		auto file_path = std::filesystem::path(
			static_cast<std::string>(config_lib::config.get("WORKING_DIR")));

		if (request.method.target.size() == 1)
		{
			file_path /= "index.html";
		}
		else
		{
			file_path /= request.method.target.substr(1);
		}

		std::ifstream f(file_path, std::ios::binary);

		if (!f.is_open())
		{
			log_and_throw(
				std::format("Failed to open {}", path_to_c_str(file_path)));
		}

		co_await sock_stream_.send_all_async(
			"HTTP/1.1 200 OK\r\n"s); // Raw strings include a NUL thus don't
									 // work

		f.seekg(0, std::ios::end);
		const std::streamoff file_size = f.tellg();
		f.seekg(0, std::ios::beg);

		co_await sock_stream_.send_all_async(
			std::format("Content-Length: {}\r\n", file_size));

		std::vector<char> buf;
		buf.resize(fstream_buf_size);

		co_await sock_stream_.send_all_async("\r\n"s);

		while (f.read(buf.data(), static_cast<std::streamsize>(buf.size())))
		{
			co_await sock_stream_.send_all_async(buf);
		}

		co_await sock_stream_.send_all_async(
			{buf.begin(), std::next(buf.begin(), f.gcount())});
	}

	async_lib::Task<void> Server::run()
	{
		while (true)
		{
			Request request;
			try
			{
				request = co_await get_request();
			}
			catch (std::exception &e)
			{
				spdlog::debug("get_request() failed : {}", e.what());
				break;
			}

			try
			{
				co_await send_response(request);
			}
			catch (std::exception &e)
			{
				spdlog::debug("send_response() failed : {}", e.what());
				break;
			}

			co_await async_lib::Task<void>::ScheduleResume();
		}

		spdlog::debug("The server loop has ended");
	}
} // namespace http_lib