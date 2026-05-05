#pragma once

#include <deque>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <winsock2.h>

#include "async.hpp"

namespace socket_lib
{
	class SockWrapper
	{
	  public:
		SockWrapper(SOCKET sock);
		~SockWrapper();

		SockWrapper(SockWrapper const &other) = delete;
		SockWrapper(SockWrapper &&other) noexcept;

		SockWrapper &operator=(SockWrapper const &other) = delete;
		SockWrapper &operator=(SockWrapper &&other) noexcept;

		[[nodiscard]] SOCKET get() const;

	  private:
		bool is_moved_from_ = false;
		SOCKET sock_;
	};

	class SockStream
	{
	  public:
		explicit SockStream(SockWrapper sock, int buf_size);

		std::vector<char> read_no_lock(int size);
		std::vector<char> read(int size);
		async_lib::Task<std::vector<char>> read_all_async_no_lock(int size);
		async_lib::Task<std::vector<char>> read_all_async(int size);
		async_lib::Task<std::string> read_line_async();

		[[nodiscard]] int send(const std::span<const char> &data) const;
		[[nodiscard]] async_lib::Task<void> send_all_async(
			const std::span<const char> &data) const;

	  private:
		int buf_size_;
		SockWrapper sock_;
		std::deque<char> read_queue_;
		std::unique_ptr<std::mutex> read_queue_mutex_;
	};
} // namespace socket_lib