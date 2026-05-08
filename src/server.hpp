#pragma once

#include "config.hpp"

#include <atomic>
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
		std::unordered_map<std::string, std::string> queries;
		std::string http_version;
	};

	struct Request
	{
		Method method;
		std::unordered_map<std::string, std::string> headers;
		std::vector<char> body;
	};

	enum class ErrorStatusCode : int
	{
		FORBIDDEN,
		NOT_FOUND,
		METHOD_NOT_ALLOWED
	};

	class Server final
	{
	  public:
		explicit Server(
			socket_lib::SockWrapper sock,
			int sock_stream_buf_size = default_sock_stream_buf_size);
		Server(const Server &other) = delete;
		Server(Server &&other) = delete;

		[[nodiscard]] async_lib::Task<void> run();
		void resume() const;
		[[nodiscard]] bool is_done() const;
		static void start_spawning_servers(config_lib::Config server_config);
		static void stop_spawning_servers();
		static void join_server_spawner();

	  private:
		static constexpr int default_sock_stream_buf_size = 100;
		socket_lib::SockStream sock_stream_;
		async_lib::Task<void> runner_task_;
		static socket_lib::SockWrapper listen_sock;

		[[nodiscard]] static config_lib::Config &get_config();
		[[nodiscard]] async_lib::Task<Request> get_request();
		[[nodiscard]] async_lib::Task<void> send_response(
			const Request &request) const;
		[[nodiscard]] async_lib::Task<void> send_error_status_code(
			ErrorStatusCode status_code) const;
		static void start_spawn_servers_func(
			socket_lib::SockWrapper listen_sock);

		static std::thread server_spawner;
		static std::atomic<bool> spawn_servers;
	};
} // namespace http_lib
