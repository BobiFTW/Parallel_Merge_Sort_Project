#ifndef _THREAD_POOL
#define _THREAD_POOL

#include <vector>
#include <mutex>
#include <queue>
#include <future>
#include <functional>


class ThreadPool {
public:
	ThreadPool(std::size_t nthreads) {
		start(nthreads);
	}

	~ThreadPool() {
		stop();
	}

	template <class T>
	auto enqueue(T task) {
		std::shared_ptr <std::packaged_task<decltype(task()) ()>> wrapper
			= std::make_shared<std::packaged_task<decltype(task()) ()>>(std::move(task));
	
		{
			std::unique_lock<std::mutex> lock{ mutex };
			tasks.emplace([=] {
				(*wrapper) ();
				});
		}

		condition.notify_one();
		return wrapper->get_future();
	}

private:
	std::vector<std::thread> workers;

	std::condition_variable condition;

	std::mutex mutex;

	bool bShouldStop = false;

	std::queue<std::function<void()>> tasks;

	void start(std::size_t nthreads) {
		for (int i = 0u; i < nthreads; i++) {
			workers.emplace_back([=] {
				while (true) {
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock{ mutex };

						condition.wait(lock, [=] { return bShouldStop || !tasks.empty(); });
						if (bShouldStop && tasks.empty())
							break;

						task = std::move(tasks.front());
						tasks.pop();
					}

					task();
				}
				});
		}
	}

	void stop() noexcept {
		{
			std::unique_lock<std::mutex> lock{ mutex };
			bShouldStop = true;
		}

		condition.notify_all();

		for (std::thread& thread : workers) {
			thread.join();
		}
	}

};

#endif