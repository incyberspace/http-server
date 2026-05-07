#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

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

    inline std::jthread server_spawner;
    inline std::atomic<bool> spawn_servers = true;
    void start_spawning_servers();
    void stop_spawning_servers();

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

      private:
        static constexpr int default_sock_stream_buf_size = 100;
        socket_lib::SockStream sock_stream_;
        async_lib::Task<void> runner_task_;
        [[nodiscard]] async_lib::Task<Request> get_request();
        [[nodiscard]] async_lib::Task<void> send_response(
            const Request &request) const;
        [[nodiscard]] async_lib::Task<void> send_error_status_code(
            ErrorStatusCode status_code) const;
    };
} // namespace http_lib
