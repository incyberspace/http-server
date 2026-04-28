#pragma once

#include <deque>
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

		std::vector<char> read(int size);
		async_lib::AsyncIo<std::vector<char>> read_all_async(int size);
		std::vector<char> read_all(int size);
		std::string read_line();

		[[nodiscard]] int send(const std::span<const char> &data) const;
		void send_all(const std::span<const char> &data) const;

	  private:
		int buf_size_;
		SockWrapper sock_;
		std::deque<char> read_queue_;
	};
} // namespace socket_lib