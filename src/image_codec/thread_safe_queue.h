#pragma once

#include <queue>
#include <optional>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
public:
	ThreadSafeQueue(size_t max_size = 0) : m_max_size(max_size) {}

	template <typename U>
	void Push(U&& u) {
		std::unique_lock<std::mutex> lock(m_mtx);
		m_cv_in.wait(lock, [this] { return !m_max_size || m_queue.size() < m_max_size; });
		m_queue.emplace(std::in_place, std::forward<U>(u));
		m_cv_out.notify_one();
	}

	template <typename ...U>
	void Emplace(U&&... u) {
		std::unique_lock<std::mutex> lock(m_mtx);
		m_cv_in.wait(lock, [this] { return !m_max_size || m_queue.size() < m_max_size; });
		m_queue.emplace(std::in_place, std::forward<U>(u)...);
		m_cv_out.notify_one();
	}

	void PushNull() {
		std::unique_lock<std::mutex> lock(m_mtx);
		m_cv_in.wait(lock, [this] { return !m_max_size || m_queue.size() < m_max_size; });
		m_queue.push(std::nullopt);
		m_cv_out.notify_one();
	}

	std::optional<T> Front() {
		std::unique_lock<std::mutex> lock(m_mtx);
		m_cv_out.wait(lock, [this] { return !m_queue.empty(); });
		return m_queue.front();
	}

	std::optional<T> Pop() {
		std::unique_lock<std::mutex> lock(m_mtx);
		m_cv_out.wait(lock, [this] { return !m_queue.empty(); });
		auto t = std::move(m_queue.front());
		m_queue.pop();
		m_cv_in.notify_one();
		return t;
	}

private:
	std::queue<std::optional<T>> m_queue;
	size_t m_max_size = 0;
	std::mutex m_mtx;
	std::condition_variable m_cv_in;
	std::condition_variable m_cv_out;
};