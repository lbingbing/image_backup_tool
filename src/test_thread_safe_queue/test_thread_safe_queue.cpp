#include <iostream>
#include <thread>

#include "image_codec.h"

void producer(ThreadSafeQueue<int>& q, int num) {
    for (int i = 0; i < num; ++i) {
        q.Push(i);
    }
    q.PushNull();
}

void consumer(ThreadSafeQueue<int>& q) {
    int last = -1;
    while (true) {
        auto i = q.Pop();
        if (!i) break;
        last = i.value();
    }
    std::cout << last << "\n";
}

int main() {
    int num = 1024;
    ThreadSafeQueue<int> q(4);
    std::thread producer_thread(producer, std::ref(q), num);
    std::thread consumer_thread(consumer, std::ref(q));
    producer_thread.join();
    consumer_thread.join();

    return 0;
}
