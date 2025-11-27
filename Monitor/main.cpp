#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <memory>

struct Event {
	int id;

	Event(int id) : id(id) {}
};

std::mutex g_mtx;
std::condition_variable g_cv;
bool g_hasEvent = false;
std::shared_ptr<Event> g_event;
const int MAX = 10;

void producerThread() {
	for (int i = 0; i < MAX; ++i) {
		std::this_thread::sleep_for(std::chrono::seconds(1));

		auto ev = std::make_shared<Event>(i);
		std::unique_lock<std::mutex> lock(g_mtx);

		g_cv.wait(lock, []() {return !g_hasEvent;});

		g_event = ev;
		g_hasEvent = true;

		std::cout << "Producer: event sent" << std::endl;

		g_cv.notify_one();
	}
}

void consumerThread() {
	for (int i = 0; i < MAX; ++i) {
		std::unique_lock<std::mutex> lock(g_mtx);
		g_cv.wait(lock, []() {return g_hasEvent;});
		
		auto ev = g_event;
		g_event.reset();
		g_hasEvent = false;

		std::cout << "Consumer: event " << ev->id << " processed" << std::endl;

		g_cv.notify_one();
	}
}

int main() {
	std::thread producer(producerThread);
	std::thread consumer(consumerThread);

	producer.join();
	consumer.join();

	std::cout << "End" << std::endl;

	return 0;
}