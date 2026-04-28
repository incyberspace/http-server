#include "async.hpp"

namespace async_lib
{
	std::suspend_never AsyncPromiseBase::initial_suspend() noexcept
	{
		return {};
	}

	std::suspend_always AsyncPromiseBase::final_suspend() noexcept
	{
		return {};
	}

	void AsyncPromiseBase::unhandled_exception() const
	{
	}

	EventLoopManager::EventLoopManager()
	{
		run();
	}

	EventLoopManager &EventLoopManager::get_instance()
	{
		static EventLoopManager instance;
		return instance;
	}

	void EventLoopManager::add_async(const Handle &caller_handle)
	{
		std::lock_guard<std::mutex> lock(async_handles_mutex_);
		async_handles_.push_back(caller_handle);
	}

	void EventLoopManager::run()
	{
		while (true)
		{
			std::lock_guard<std::mutex> lock(async_handles_mutex_);

			for (auto it = async_handles_.begin(); it != async_handles_.end();)
			{
				if (it->done())
				{
					it->promise().caller_handle_.resume();
					it = async_handles_.erase(it);
					continue;
				}

				it->resume();
				it++;
			}
		}
	}
} // namespace async_lib
