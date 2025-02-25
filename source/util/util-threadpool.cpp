// Copyright (C) 2020-2022 Michael Fabian Dirks
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

#include "util-threadpool.hpp"
#include "common.hpp"
#include "util/util-logging.hpp"

#include "warning-disable.hpp"
#include <cstddef>
#include "warning-enable.hpp"

#include "warning-disable.hpp"
#if defined(D_PLATFORM_WINDOWS)
#include <Windows.h>
#elif defined(D_PLATFORM_LINUX)
#include <pthread.h>
#endif
#include "warning-enable.hpp"

#ifdef _DEBUG
#define ST_PREFIX "<%s> "
#define D_LOG_ERROR(x, ...) P_LOG_ERROR(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_WARNING(x, ...) P_LOG_WARN(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_INFO(x, ...) P_LOG_INFO(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_DEBUG(x, ...) P_LOG_DEBUG(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#else
#define ST_PREFIX "<util::threadpool> "
#define D_LOG_ERROR(...) P_LOG_ERROR(ST_PREFIX __VA_ARGS__)
#define D_LOG_WARNING(...) P_LOG_WARN(ST_PREFIX __VA_ARGS__)
#define D_LOG_INFO(...) P_LOG_INFO(ST_PREFIX __VA_ARGS__)
#define D_LOG_DEBUG(...) P_LOG_DEBUG(ST_PREFIX __VA_ARGS__)
#endif

streamfx::util::threadpool::task::task(task_callback_t callback, task_data_t data)
	: _callback(callback), _data(data), _lock(), _status_changed(), _cancelled(false), _completed(false), _failed(false)
{}

streamfx::util::threadpool::task::~task() {}

void streamfx::util::threadpool::task::run()
{
	std::lock_guard<std::mutex> lg(_lock);
	if (!_cancelled) {
		try {
			_callback(_data);
		} catch (const std::exception& ex) {
			D_LOG_ERROR("Unhandled exception in Task: %s.", ex.what());
			_failed = false;
		} catch (...) {
			D_LOG_ERROR("Unhandled exception in Task.", nullptr);
			_failed = true;
		}
	}
	_completed = true;
	_status_changed.notify_all();
}

void streamfx::util::threadpool::task::cancel()
{
	std::lock_guard<std::mutex> lg(_lock);
	_cancelled = true;
	_completed = true;
	_status_changed.notify_all();
}

bool streamfx::util::threadpool::task::is_cancelled()
{
	return _cancelled;
}

bool streamfx::util::threadpool::task::is_completed()
{
	return _completed;
}

bool streamfx::util::threadpool::task::has_failed()
{
	return _failed;
}

void streamfx::util::threadpool::task::wait()
{
	std::unique_lock<std::mutex> ul(_lock);
	if (!_cancelled && !_completed && !_failed) {
		_status_changed.wait(ul,
							 [this]() { return this->is_completed() || this->is_cancelled() || this->has_failed(); });
	}
}

void streamfx::util::threadpool::task::await_completion()
{
	wait();
}

streamfx::util::threadpool::threadpool::~threadpool()
{
	{ // Terminate all remaining tasks.
		std::lock_guard<std::mutex> lg(_tasks_lock);
		for (auto task : _tasks) {
			task->cancel();
		}
		_tasks.clear();
	}

	{ // Notify workers to stop working.
		{
			std::lock_guard<std::mutex> lg(_workers_lock);
			for (auto worker : _workers) {
				worker->stop = true;
			}
		}
		{
			std::lock_guard<std::mutex> lg(_tasks_lock);
			_tasks_cv.notify_all();
		}
		for (auto worker : _workers) {
			std::lock_guard<std::mutex> lg(worker->lifeline);
		}
	}
}

streamfx::util::threadpool::threadpool::threadpool(size_t minimum, size_t maximum)
	: _limits{minimum, maximum}, _workers_lock(), _workers(), _tasks_lock(), _tasks_cv(), _tasks()
{
	// Spawn the minimum number of threads.
	spawn(_limits.first);
}

std::shared_ptr<streamfx::util::threadpool::task>
	streamfx::util::threadpool::threadpool::push(task_callback_t callback, task_data_t data /*= nullptr*/)
{
	std::lock_guard<std::mutex> lg(_tasks_lock);
	constexpr size_t            threshold = 3;

	// Enqueue the new task.
	auto task = std::make_shared<streamfx::util::threadpool::task>(callback, data);
	_tasks.emplace_back(task);

	// Spawn additional workers if the number of queued tasks exceeds a threshold.
	if (_tasks.size() > (threshold * _worker_count)) {
		spawn(_tasks.size() / threshold);
	}

	// Return handle to caller.
	return task;
}

void streamfx::util::threadpool::threadpool::pop(std::shared_ptr<task> task)
{
	if (task) {
		task->cancel();
	}
	std::lock_guard<std::mutex> lg(_tasks_lock);
	_tasks.remove(task);
}

void streamfx::util::threadpool::threadpool::spawn(size_t count)
{
	std::lock_guard<std::mutex> lg(_workers_lock);
	for (size_t n = 0; (n < count) && (_worker_count < _limits.second); n++) {
		auto wi            = std::make_shared<worker_info>();
		wi->stop           = false;
		wi->last_work_time = std::chrono::high_resolution_clock::now();
		wi->thread         = std::thread(std::bind(&streamfx::util::threadpool::threadpool::work, this, wi));
		wi->thread.detach();
		_workers.emplace_back(wi);
		++_worker_count;
		D_LOG_DEBUG("Spawning new worker thread (%zu < %zu < %zu).", _limits.first, _worker_count.load(),
					_limits.second);
	}
}

bool streamfx::util::threadpool::threadpool::die(std::shared_ptr<worker_info> wi)
{
	constexpr std::chrono::seconds delay{1};

	std::lock_guard<std::mutex> lg(_workers_lock);
	bool                        result = false;

	if (_worker_count > _limits.first) {
		auto now = std::chrono::high_resolution_clock::now();
		result   = ((wi->last_work_time + delay) <= now) && ((_last_worker_death + delay) <= now);

		if (result) {
			_last_worker_death = now;
			--_worker_count;
			_workers.remove(wi);
			D_LOG_DEBUG("Terminated idle worker thread (%zu < %zu < %zu).", _limits.first, _worker_count.load(),
						_limits.second);
		}
	}

	return result;
}

void streamfx::util::threadpool::threadpool::work(std::shared_ptr<worker_info> wi)
{
	std::shared_ptr<streamfx::util::threadpool::task> task{};
	std::lock_guard<std::mutex>                       lg(wi->lifeline);

#if defined(D_PLATFORM_WINDOWS)
	SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN | THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadDescription(GetCurrentThread(), L"StreamFX Worker Thread");
#elif defined(D_PLATFORM_LINUX)
	struct sched_param param;
	param.sched_priority = 0;
	pthread_setschedparam(pthread_self(), SCHED_IDLE, &param);
	pthread_setname_np(pthread_self(), "StreamFX Worker Thread");
#endif

	while (!wi->stop) {
		{ // Try and acquire new work.
			std::unique_lock<std::mutex> ul(_tasks_lock);

			// Is there any work available right now?
			if (_tasks.size() == 0) { // If not:
				// Block this thread until it is notified of a change.
				_tasks_cv.wait_until(
					ul,
					std::chrono::time_point(std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(250)),
					[this, wi]() { return wi->stop || _tasks.size() > 0; });
			}

			// If we were asked to stop, skip everything.
			if (wi->stop) {
				continue;
			}

			// If there is work to be done, take it.
			if (_tasks.size() > 0) {
				wi->last_work_time = std::chrono::high_resolution_clock::now();
				task               = _tasks.front();
				_tasks.pop_front();
			} else if (die(wi)) { // Is the threadpool requesting less threads?
				break;
			}
		}

		if (task) {
			task->run();
			task.reset();
		}
	}
}
