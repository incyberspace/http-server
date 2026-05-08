#include "async.hpp"

#include "server.hpp"
#include "utils.hpp"

namespace async_lib
{
	FinalAwaitable::FinalAwaitable(
		const std::optional<Handle> handle_to_be_resumed)
	{
		handle_to_be_resumed_ = handle_to_be_resumed;
	}

	[[nodiscard]] FinalAwaitable::Handle FinalAwaitable::await_suspend(
		Handle) const noexcept
	{
		if (handle_to_be_resumed_.has_value())
		{
			return *handle_to_be_resumed_;
		}

		return std::noop_coroutine();
	}

	bool FinalAwaitable::await_ready() const noexcept
	{
		return false;
	}

	void FinalAwaitable::await_resume() const noexcept
	{
	}

	EventLoopManager::EventLoopManager()
	{
		start_event_loop();
	}

	EventLoopManager::~EventLoopManager()
	{
		if (run_event_loop_flag)
		{
			runner_.join();
		}
	}

	EventLoopManager &EventLoopManager::get_instance()
	{
		static EventLoopManager instance;
		return instance;
	}

	void EventLoopManager::add_handle(const Handle handle)
	{
		SPDLOG_DEBUG("Adding a handle");
		{
			std::lock_guard lock(handles_mutex_);
			handles_.emplace_back(handle);
		}

		wake_up_event_loop();
	}

	void EventLoopManager::wake_up_event_loop()
	{
		SPDLOG_DEBUG("Waking up the event loop");
		cv_.notify_one();
	}

	void EventLoopManager::start_event_loop()
	{
		run_event_loop_flag = true;
		runner_ = std::thread(&EventLoopManager::run, this);
	}

	void EventLoopManager::end_event_loop()
	{
		run_event_loop_flag = false;
		wake_up_event_loop();
		runner_.join();
	}

	void EventLoopManager::run()
	{
		while (true)
		{
			std::unique_lock lock(handles_mutex_);
			while (!handles_.empty() && run_event_loop_flag)
			{
				for (auto it = handles_.begin(); it != handles_.end();)
				{
					if (it->done())
					{
						it = handles_.erase(it);
						continue;
					}

					if (it->promise().ready_to_be_resumed_)
					{
						const auto tmp = *it;
						handles_.erase(it);

						SPDLOG_DEBUG("Resuming a handle");

						lock.unlock(); // To avoid recursive locking vvv
						tmp.resume();  // The resumed coroutine might add a
									   // handle
						lock.lock();

						break; // Break since handles_ might've been modified
					}

					it++;
				}
			}

			if (!run_event_loop_flag)
			{
				break;
			}

			SPDLOG_DEBUG("Putting the event loop to sleep");
			cv_.wait(lock);
		}
	}
} // namespace async_lib
