#pragma once

#include <concepts>
#include <coroutine>
#include <spdlog/spdlog.h>
#include <utility>

#include <iostream>

namespace async_lib
{
	template <std::copyable T> class Awaitable
	{
	  public:
		struct promise_type
		{
			Awaitable get_return_object()
			{
				return Awaitable{Handle::from_promise(*this)};
			}

			auto yield_value(T value)
			{
				cur_value_ = std::move(value);
				return std::suspend_always();
			}

			void return_value(T value)
			{
				cur_value_ = std::move(value);
			}

			T cur_value_;
		};

	  private:
		using Handle = std::coroutine_handle<promise_type>;
		Handle self_handle_;
		Handle caller_handle_;

	  public:
		explicit Awaitable(Handle handle) : self_handle_(std::move(handle))
		{
		}
		Awaitable(Awaitable &other) = delete;
		Awaitable(Awaitable &&other) = delete;

		~Awaitable()
		{
			self_handle_.destroy();
		}

		[[nodiscard]] bool await_ready() const
		{
			return false;
		}

		void await_suspend(const Handle &caller_handle)
		{
			caller_handle_ = caller_handle;
		}

		T await_resume()
		{
			caller_handle_.resume();
			return self_handle_.promise().cur_value_;
		}
	};

	struct AsyncPromiseBase
	{
		std::suspend_never initial_suspend() noexcept;
		std::suspend_always final_suspend() noexcept;
		void unhandled_exception() const;

		std::coroutine_handle<AsyncPromiseBase> caller_handle_;
	};

	class EventLoopManager
	{
	  public:
		EventLoopManager(const EventLoopManager &other) = delete;
		EventLoopManager(EventLoopManager &&other) = delete;

		[[nodiscard]] static EventLoopManager &get_instance();

	  private:
		using Handle = std::coroutine_handle<AsyncPromiseBase>;

		EventLoopManager();
		void run();
		void add_async(const Handle &caller_handle);

		std::vector<Handle> async_handles_;
		std::mutex async_handles_mutex_;
	};

	template <std::copyable T> class AsyncIo
	{
	  public:
		struct promise_type : AsyncPromiseBase
		{
			AsyncIo get_return_object()
			{
				return AsyncIo{Handle::from_promise(*this)};
			}

			auto yield_value(void) const
			{
				return std::suspend_always();
			}

			void return_value(T value)
			{
				cur_value_ = std::move(value);
			}

			T cur_value_;
		};

	  private:
		using Handle = std::coroutine_handle<promise_type>;
		Handle self_handle_;
		Handle caller_handle_;

	  public:
		explicit AsyncIo(Handle handle) : self_handle_(std::move(handle))
		{
		}
		AsyncIo(AsyncIo &other) = delete;
		AsyncIo(AsyncIo &&other) = delete;

		~AsyncIo()
		{
			self_handle_.destroy();
		}

		bool await_ready() const
		{
			return false;
		}

		void await_suspend(const Handle &caller_handle)
		{
			self_handle_.promise().caller_handle_ = caller_handle;
			EventLoopManager::get_instance().add_async(caller_handle_);
		}

		T await_resume()
		{
			return self_handle_.promise().cur_value_;
		}
	};
} // namespace async_lib