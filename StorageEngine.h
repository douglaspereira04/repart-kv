#pragma once

#include <string>
#include <string_view>
#include <shared_mutex>
#include <vector>

/**
 * @brief Abstract base class for storage engines
 * @tparam T The type of the underlying storage engine
 */
template<typename T>
class StorageEngine {
protected:
    T* _storage_engine;  // Pointer to the underlying storage engine
    mutable std::shared_mutex _lock;  // Mutex for thread-safe operations

public:
    /**
     * @brief Constructor
     * @param engine Pointer to the storage engine implementation
     */
    explicit StorageEngine(T* engine) : _storage_engine(engine) {}

    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~StorageEngine() = default;

    /**
     * @brief Read a value by key
     * @param key The key to read
     * @return The value associated with the key
     */
    virtual std::string read(const std::string& key) = 0;

    /**
     * @brief Write a key-value pair
     * @param key The key to write
     * @param value The value to associate with the key
     */
    virtual void write(const std::string& key, const std::string& value) = 0;

    /**
     * @brief Scan for keys starting with a given prefix
     * @param key_prefix The prefix to search for
     * @param limit Maximum number of keys to return
     * @return Vector of keys matching the prefix
     */
    virtual std::vector<std::string> scan(const std::string& key_prefix, size_t limit) = 0;

    /**
     * @brief Acquire a shared lock (for read operations)
     */
    void lock_shared() const {
        _lock.lock_shared();
    }

    /**
     * @brief Release a shared lock
     */
    void unlock_shared() const {
        _lock.unlock_shared();
    }

    /**
     * @brief Acquire an exclusive lock (for write operations)
     */
    void lock() const {
        _lock.lock();
    }

    /**
     * @brief Release an exclusive lock
     */
    void unlock() const {
        _lock.unlock();
    }
};
