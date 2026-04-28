#include "socket.hpp"

#include "async.hpp"
#include "utils.hpp"

#include <cassert>
#include <ranges>
#include <utility>

namespace socket_lib
{
	SockWrapper::SockWrapper(const SOCKET sock) : sock_(sock)
	{
	}

	SockWrapper::~SockWrapper()
	{
		if (!is_moved_from_)
		{
			closesocket(sock_);
		}
	}

	SockWrapper::SockWrapper(SockWrapper &&other) noexcept : sock_(other.sock_)
	{
		other.is_moved_from_ = true;
	}

	SockWrapper &SockWrapper::operator=(SockWrapper &&other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}

		sock_ = other.sock_;
		other.is_moved_from_ = true;

		return *this;
	}

	SOCKET SockWrapper::get() const
	{
		return sock_;
	}

	SockStream::SockStream(SockWrapper sock, const int buf_size)
		: buf_size_(buf_size), sock_(std::move(sock))
	{
	}

	std::vector<char> SockStream::read(const int size)
	{
		const int bytes_to_read_from_queue = static_cast<int>(
			static_cast<int>(read_queue_.size()) > size ? size
														: read_queue_.size());
		std::vector<char> result;

		result.insert(result.end(), read_queue_.begin(),
					  std::next(read_queue_.begin(), bytes_to_read_from_queue));
		read_queue_.erase(
			read_queue_.begin(),
			std::next(read_queue_.begin(), bytes_to_read_from_queue));

		const int remaining_bytes = size - bytes_to_read_from_queue;
		assert(remaining_bytes >= 0);

		std::vector<char> buf;
		buf.resize(remaining_bytes);

		const int read_bytes =
			recv(sock_.get(), buf.data(), remaining_bytes, 0);

		if (read_bytes < 0)
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				return result;
			}

			log_and_throw(std::format("recv failed. Reason : {}",
									  utils_lib::get_last_error_as_str()));
		}

		result.insert(result.end(), buf.begin(),
					  std::next(buf.begin(), read_bytes));

		return result;
	}

	async_lib::AsyncIo<std::vector<char>> SockStream::read_all_async(const int size)
	{
		int total_read_bytes = 0;
		std::vector<char> result;

		while (total_read_bytes < size)
		{
			const int remaining_bytes = size - total_read_bytes;
			std::vector<char> buf =
				read(remaining_bytes > buf_size_ ? buf_size_ : remaining_bytes);
			total_read_bytes += static_cast<int>(buf.size());
			result.insert(result.end(), buf.begin(), buf.end());

			co_await std::suspend_always();
		}
		assert(total_read_bytes == size);

		co_return result;
	}

	std::vector<char> SockStream::read_all(const int size)
	{
		int total_read_bytes = 0;
		std::vector<char> result;

		while (total_read_bytes < size)
		{
			const int remaining_bytes = size - total_read_bytes;
			std::vector<char> buf =
				read(remaining_bytes > buf_size_ ? buf_size_ : remaining_bytes);
			total_read_bytes += static_cast<int>(buf.size());
			result.insert(result.end(), buf.begin(), buf.end());
		}
		assert(total_read_bytes == size);

		return result;
	}

	std::string SockStream::read_line()
	{
		std::string result;

		while (true)
		{
			const std::vector<char> buf = read(buf_size_);

			if (const auto cr_it =
					std::ranges::find(buf.begin(), buf.end(), '\r');
				cr_it != buf.end())
			{
				result.insert(result.end(), buf.begin(), cr_it);

				if (const auto lf_it =
						std::ranges::find(buf.begin(), buf.end(), '\n');
					lf_it == buf.end())
				{
					read_all(1); // Reads '\n' from the socket
					read_queue_.insert(read_queue_.end(), std::next(cr_it, 1),
									   buf.end());
				}
				else
				{
					read_queue_.insert(read_queue_.end(), std::next(cr_it, 2),
									   buf.end());
				}

				break;
			}

			if (const auto lf_it =
					std::ranges::find(buf.begin(), buf.end(), '\n');
				lf_it != buf.end())
			{
				result.insert(result.end(), buf.begin(), lf_it);
				read_queue_.insert(read_queue_.end(), std::next(lf_it),
								   buf.end());
				break;
			}
		}

		return result;
	}

	int SockStream::send(const std::span<const char> &data) const
	{
		const int sent_bytes =
			::send(sock_.get(), data.data(), static_cast<int>(data.size()), 0);

		if (sent_bytes < 0)
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				return 0;
			}

			log_and_throw(std::format("send failed. Reason : {}",
									  utils_lib::get_last_error_as_str()));
		}

		return sent_bytes;
	}

	void SockStream::send_all(const std::span<const char> &data) const
	{
		int total_sent_bytes = 0;

		while (total_sent_bytes < static_cast<int>(data.size()))
		{
			const int remaining_bytes =
				static_cast<int>(data.size()) - total_sent_bytes;
			total_sent_bytes +=
				send({std::next(data.begin(), total_sent_bytes),
					  std::next(data.begin(),
								total_sent_bytes + (buf_size_ > remaining_bytes
														? remaining_bytes
														: buf_size_))});
		}
		assert(total_sent_bytes == static_cast<int>(data.size()));
	}
} // namespace socket_lib
