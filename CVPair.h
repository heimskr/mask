#pragma once

#include <condition_variable>
#include <mutex>

struct CVPair {
	std::mutex mutex;
	std::condition_variable var;

	template <typename... Args>
	void wait(Args &&...args) {
		std::unique_lock lock(mutex);
		var.wait(lock, args...);
	}

	template <typename D>
	bool wait_for(const D &duration) {
		std::unique_lock lock(mutex);
		return var.wait_for(lock, duration) == std::cv_status::no_timeout;
	}

	template <typename D, typename P>
	bool wait_for(const D &duration, const P &predicate) {
		std::unique_lock lock(mutex);
		return var.wait_for(lock, duration, predicate);
	}

	template <typename D>
	bool wait_until(const D &duration) {
		std::unique_lock lock(mutex);
		return var.wait_until(lock, duration) == std::cv_status::no_timeout;
	}

	template <typename D, typename P>
	bool wait_until(const D &duration, const P &predicate) {
		std::unique_lock lock(mutex);
		return var.wait_until(lock, duration, predicate);
	}

	void notify() {
		var.notify_all();
	}
};
