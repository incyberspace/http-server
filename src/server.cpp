#include "server.hpp"

#include <fstream>
#include <iostream>
#include <ranges>
#include <regex>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "async.hpp"
#include "config.hpp"

namespace
{
	[[nodiscard]] bool is_directory_valid(
		const std::filesystem::path &working_dir,
		const std::filesystem::path &dir_to_be_checked);

	bool is_directory_valid(const std::filesystem::path &working_dir,
							const std::filesystem::path &dir_to_be_checked)
	{
		if (working_dir == dir_to_be_checked)
		{
			return true;
		}

		while (true)
		{
			const std::filesystem::path parent_dir = working_dir.parent_path();

			if (parent_dir == working_dir)
			{
				return true;
			}

			if (parent_dir == working_dir.root_directory())
			{
				break;
			}
		}

		return false;
	}

} // namespace

namespace http_lib
{
	std::thread Server::server_spawner;
	std::atomic<bool> Server::spawn_servers = true;
	socket_lib::SockWrapper Server::listen_sock;

	config_lib::Config &Server::get_config()
	{
		static config_lib::Config config;
		return config;
	}

	void Server::start_spawn_servers_func(socket_lib::SockWrapper listen_sock)
	{
		std::vector<std::unique_ptr<Server>> spawned_servers;

		while (spawn_servers)
		{
			SPDLOG_DEBUG("Server count : {}", spawned_servers.size());
			socket_lib::SockWrapper accept_sock =
				accept(listen_sock.get(), nullptr, nullptr);

			if (accept_sock.get() == INVALID_SOCKET)
			{
				SPDLOG_ERROR("Accept failed. Reason : {}",
							 utils_lib::get_last_error_as_str());
				break;
			}

			u_long socket_mode = 1;
			if (ioctlsocket(accept_sock.get(), FIONBIO, &socket_mode) != 0)
			{
				SPDLOG_ERROR("ioctlsocket failed. Reason : {}",
							 utils_lib::get_last_error_as_str());
				break;
			}

			//std::erase_if(spawned_servers,
			//			  [](const std::unique_ptr<Server> &server) -> bool {
			//				  return server->is_done();
			//			  });
			SPDLOG_INFO("Accepted");
			spawned_servers.emplace_back(
				std::make_unique<Server>(std::move(accept_sock), 100000));
		}
	}

	void Server::start_spawning_servers(config_lib::Config server_config)
	{
		get_config() = std::move(server_config);

		listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listen_sock.get() == INVALID_SOCKET)
		{
			log_and_throw(
				std::format("Could not create listening socket. Reason : {}",
							utils_lib::get_last_error_as_str()));
		}

		sockaddr_in addr_info = {};
		addr_info.sin_family = AF_INET;
		addr_info.sin_port = htons(static_cast<int>(get_config().get("PORT")));
		inet_pton(AF_INET,
				  static_cast<std::string>(get_config().get("LISTEN")).c_str(),
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

		server_spawner =
			std::thread(start_spawn_servers_func, std::move(listen_sock));
	}

	void Server::stop_spawning_servers()
	{
		listen_sock.close();
		spawn_servers = false;
	}

	void Server::join_server_spawner()
	{
		server_spawner.join();
	}

	Server::Server(socket_lib::SockWrapper sock, const int sock_stream_buf_size)
		: sock_stream_(std::move(sock), sock_stream_buf_size),
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

		if (request.method.type != "GET")
		{
			co_await send_error_status_code(
				ErrorStatusCode::METHOD_NOT_ALLOWED);
			co_return;
		}

		std::filesystem::path working_dir =
			static_cast<std::string>(get_config().get("WORKING_DIR"));
		working_dir = std::filesystem::canonical(working_dir);
		auto file_path = working_dir;

		if (request.method.target.substr(1).empty())
		{
			file_path /= "index.html";
		}
		else
		{
			file_path = request.method.target.substr(1);

			bool is_path_not_found = false;

			try
			{
				file_path = std::filesystem::canonical(file_path);
			}
			catch (const std::exception &)
			{
				is_path_not_found = true;
			}

			if (is_path_not_found)
			{
				co_await send_error_status_code(ErrorStatusCode::NOT_FOUND);
				co_return;
			}

			if (file_path == working_dir)
			{
				file_path /= "index.html";
			}
		}

		if (is_directory(file_path) ||
			!is_directory_valid(working_dir, file_path.parent_path()))
		{
			co_await send_error_status_code(ErrorStatusCode::FORBIDDEN);
			co_return;
		}

		std::ifstream f(file_path, std::ios::binary);

		// Might be redundant not sure
		if (!f.is_open())
		{
			co_await send_error_status_code(ErrorStatusCode::NOT_FOUND);
			co_return;
		}

		co_await sock_stream_.send_all_async("HTTP/1.1 200 OK\r\n"s);

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

	async_lib::Task<void> Server::send_error_status_code(
		const ErrorStatusCode status_code) const
	{
		using namespace std::string_literals;

		std::string msg;

		switch (status_code)
		{
		case ErrorStatusCode::FORBIDDEN:
			msg = "403 Forbidden";
			break;
		case ErrorStatusCode::NOT_FOUND:
			msg = "404 Not Found";
			break;
		case ErrorStatusCode::METHOD_NOT_ALLOWED:
			msg = "405 Method Not Allowed";
			break;
		default:
			std::unreachable();
		}

		co_await sock_stream_.send_all_async("HTTP/1.1 " + msg + "\r\n");
		co_await sock_stream_.send_all_async("Content-Length: 0\r\n"s);
		co_await sock_stream_.send_all_async("\r\n"s);
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
			catch (const std::exception &)
			{
				SPDLOG_DEBUG("get_request() failed");
				break;
			}

			try
			{
				co_await send_response(request);
			}
			catch (const std::exception &)
			{
				SPDLOG_DEBUG("send_response() failed");
				break;
			}

			co_await async_lib::Task<void>::ScheduleResume();
		}

		SPDLOG_DEBUG("The server loop has ended");
	}
} // namespace http_lib
