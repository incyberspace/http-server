#pragma once

#include <coroutine>
#include <deque>
#include <spdlog/spdlog.h>
#include <utility>

namespace async_lib
{
	struct FinalAwaitable final
	{
	  private:
		using Handle = std::coroutine_handle<>;
		std::optional<Handle> handle_to_be_resumed_;

	  public:
		explicit FinalAwaitable(
			std::optional<Handle> handle_to_be_resumed = std::nullopt);
		[[nodiscard]] Handle await_suspend(Handle) const noexcept;
		[[nodiscard]] bool await_ready() const noexcept;
		void await_resume() const noexcept;
	};

	struct PromiseBase
	{
		std::suspend_never initial_suspend() noexcept
		{
			return {};
		}

		std::suspend_always final_suspend() noexcept
		{
			return {};
		}

		void unhandled_exception()
		{
			except_ = std::current_exception();
		}

		[[nodiscard]] auto yield_value() const
		{
			return std::suspend_always();
		}

		std::optional<std::coroutine_handle<>> caller_handle_;
		std::exception_ptr except_;
		bool ready_to_be_resumed_ = false;
	};

	class EventLoopManager final
	{
		template <typename T> friend class Task;
		friend struct ScheduleResume;

	  public:
		EventLoopManager(const EventLoopManager &other) = delete;
		EventLoopManager(EventLoopManager &&other) = delete;

		[[nodiscard]] static EventLoopManager &get_instance();

	  private:
		using Handle = std::coroutine_handle<PromiseBase>;
		std::deque<Handle> handles_;
		std::mutex handles_mutex_;
		std::condition_variable cv_;
		std::jthread runner_;

		EventLoopManager();
		void run();
		void add_handle(Handle handle);
		void wake_up_event_loop();
	};

	template <typename T> class Task
	{
	  public:
		struct promise_type final : PromiseBase
		{
			std::suspend_never initial_suspend() noexcept
			{
				return {};
			}

			[[nodiscard]] FinalAwaitable final_suspend() const noexcept
			{
				return FinalAwaitable(caller_handle_);
			}

			Task get_return_object()
			{
				return Task{Handle::from_promise(*this)};
			}

			auto yield_value() const
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
		Handle handle_;
		bool is_moved_from = false;

	  public:
		struct ScheduleResume final
		{
			void await_suspend(const Handle handle) const
			{
				handle.promise().ready_to_be_resumed_ = true;
				EventLoopManager::get_instance().add_handle(
					std::coroutine_handle<PromiseBase>::from_promise(
						handle.promise()));
			}

			[[nodiscard]] bool await_ready() const
			{
				return false;
			}

			void await_resume() const
			{
			}
		};

		Task() = default;
		explicit Task(const Handle handle) : handle_(handle)
		{
		}

		Task(const Task &other) = delete;
		Task(Task &&other) noexcept : handle_(other.handle_)
		{
			other.is_moved_from = true;
		}

		Task &operator=(const Task &other) = delete;
		Task &operator=(Task &&other) = delete;

		~Task()
		{
			if (is_moved_from)
			{
				return;
			}

			handle_.destroy();
		}

		[[nodiscard]] bool await_ready() const
		{
			return false;
		}

		[[nodiscard]] std::coroutine_handle<> await_suspend(
			const std::coroutine_handle<> caller_handle) const
		{
			spdlog::debug("Suspend is called");

			if (handle_.done())
			{
				return caller_handle;
			}

			handle_.promise().caller_handle_ = caller_handle;
			return std::noop_coroutine();
		}

		T await_resume()
		{
			if (const auto except = handle_.promise().except_)
			{
				std::rethrow_exception(except);
			}

			return handle_.promise().cur_value_;
		}

		[[nodiscard]] bool is_done() const
		{
			return handle_.done();
		}

		void resume() const
		{
			assert(!is_done());
			handle_.resume();
		}
	};

	template <> class Task<void>
	{
	  public:
		struct promise_type final : PromiseBase
		{
			std::suspend_never initial_suspend() noexcept
			{
				return {};
			}

			[[nodiscard]] FinalAwaitable final_suspend() const noexcept
			{
				return FinalAwaitable(caller_handle_);
			}

			Task get_return_object()
			{
				return Task{Handle::from_promise(*this)};
			}

			[[nodiscard]] auto yield_value() const
			{
				return std::suspend_always();
			}

			void return_void() const
			{
			}

			std::optional<std::coroutine_handle<>> caller_handle_;
		};

		using Handle = std::coroutine_handle<promise_type>;

	  private:
		Handle handle_;
		bool is_moved_from = false;

	  public:
		struct ScheduleResume final
		{
			void await_suspend(const Handle handle) const
			{
				handle.promise().ready_to_be_resumed_ = true;
				EventLoopManager::get_instance().add_handle(
					std::coroutine_handle<PromiseBase>::from_promise(
						handle.promise()));
			}

			[[nodiscard]] bool await_ready() const
			{
				return false;
			}

			void await_resume() const
			{
			}
		};

		Task() = default;
		explicit Task(const Handle handle) : handle_(handle)
		{
		}

		Task(Task &&other) noexcept : handle_(other.handle_)
		{
			other.is_moved_from = true;
		}

		Task &operator=(const Task &other) = delete;
		Task &operator=(Task &&other) = delete;

		~Task()
		{
			if (is_moved_from)
			{
				return;
			}

			handle_.destroy();
		}

		[[nodiscard]] bool await_ready() const
		{
			return false;
		}

		[[nodiscard]] std::coroutine_handle<> await_suspend(
			const std::coroutine_handle<> caller_handle) const
		{
			spdlog::debug("Suspend is called");

			if (handle_.done())
			{
				return caller_handle;
			}

			handle_.promise().caller_handle_ = caller_handle;
			return std::noop_coroutine();
		}

		void await_resume() const
		{
			if (const auto except = handle_.promise().except_)
			{
				std::rethrow_exception(except);
			}
		}

		[[nodiscard]] bool is_done() const
		{
			return handle_.done();
		}

		void resume() const
		{
			assert(!is_done());
			handle_.resume();
		}
	};
} // namespace async_lib