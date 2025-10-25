#pragma once

#include <mutex>

template <typename T> class Future {
private:
    std::mutex mtx_;
    T &value_;

public:
    // Constructor takes a reference to T and locks the mutex
    explicit Future(T &value) : value_(value) { mtx_.lock(); }

    // Destructor unlocks the mutex if it's still locked
    ~Future() {
        if (mtx_.try_lock()) {
            mtx_.unlock();
        }
    }

    // Copy constructor and assignment operator are deleted
    // to prevent copying of Future objects
    Future(const Future &) = delete;
    Future &operator=(const Future &) = delete;

    // Move constructor and assignment operator are deleted
    // to prevent moving of Future objects
    Future(Future &&) = delete;
    Future &operator=(Future &&) = delete;

    // Wait method locks the mutex
    void wait() { mtx_.lock(); }

    // Notify method unlocks the mutex
    void notify() { mtx_.unlock(); }

    // Get value reference method returns reference to the value
    T &value() { return value_; }

    void value(const T &value) { value_ = value; }
};
