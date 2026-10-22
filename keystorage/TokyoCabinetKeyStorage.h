#pragma once

#include "KeyStorage.h"
#include "KeyStorageIterator.h"
#include "KeyStorageValueBinary.h"
#include <tcbdb.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

template <KeyStorageValueType ValueType> class TokyoCabinetKeyStorageIterator;

/**
 * @brief Tokyo Cabinet B+tree KeyStorage without \c tcbdbsetmutex.
 *
 * Caller-owned synchronization (kvstorage reader-writer lock).
 */
template <KeyStorageValueType ValueType> class TokyoCabinetKeyStorage
    : public KeyStorage<TokyoCabinetKeyStorage<ValueType>,
                        TokyoCabinetKeyStorageIterator<ValueType>, ValueType> {
private:
    mutable TCBDB *db_;
    bool is_open_;
    std::string db_path_;
    static std::atomic_int db_counter_;
    static std::string id_;

    static std::string copy_tc(void *p, int size) {
        if (p == nullptr) {
            return {};
        }
        std::string out(static_cast<const char *>(p),
                        static_cast<size_t>(size < 0 ? 0 : size));
        std::free(p);
        return out;
    }

    bool open_at_path(const std::string &file_path) {
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(file_path).parent_path(), ec);
        db_ = tcbdbnew();
        if (db_ == nullptr) {
            return false;
        }
        if (!tcbdbtune(db_, 32, 256, 8388608, -1, -1, BDBTLARGE)) {
            tcbdbdel(db_);
            db_ = nullptr;
            return false;
        }
        if (!tcbdbopen(db_, file_path.c_str(), BDBOWRITER | BDBOCREAT)) {
            tcbdbdel(db_);
            db_ = nullptr;
            return false;
        }
        is_open_ = true;
        return true;
    }

    void close_handles() {
        if (db_) {
            tcbdbclose(db_);
            tcbdbdel(db_);
            db_ = nullptr;
        }
        is_open_ = false;
    }

public:
    explicit TokyoCabinetKeyStorage(const std::string &base_path) :
        db_(nullptr), is_open_(false) {
        db_path_ =
            (std::filesystem::path(base_path) / "repart_kv_keystorage" / id_ /
             (std::to_string(
                  db_counter_.fetch_add(1, std::memory_order_relaxed)) +
              ".tcb"))
                .string();
        std::filesystem::create_directories(
            std::filesystem::path(db_path_).parent_path());
        (void)open_at_path(db_path_);
    }

    ~TokyoCabinetKeyStorage() {
        close_handles();
        if (!db_path_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(
                std::filesystem::path(db_path_).parent_path(), ec);
        }
    }

    TokyoCabinetKeyStorage(const TokyoCabinetKeyStorage &) = delete;
    TokyoCabinetKeyStorage &operator=(const TokyoCabinetKeyStorage &) = delete;

    TokyoCabinetKeyStorage(TokyoCabinetKeyStorage &&other) noexcept :
        db_(other.db_), is_open_(other.is_open_),
        db_path_(std::move(other.db_path_)) {
        other.db_ = nullptr;
        other.is_open_ = false;
        other.db_path_.clear();
    }

    TokyoCabinetKeyStorage &operator=(TokyoCabinetKeyStorage &&other) noexcept {
        if (this != &other) {
            close_handles();
            db_ = other.db_;
            is_open_ = other.is_open_;
            db_path_ = std::move(other.db_path_);
            other.db_ = nullptr;
            other.is_open_ = false;
            other.db_path_.clear();
        }
        return *this;
    }

    bool get_impl(const std::string &key, ValueType &value) const {
        if (!is_open_ || !db_) {
            return false;
        }
        int vsiz = 0;
        void *vbuf =
            tcbdbget(db_, key.data(), static_cast<int>(key.size()), &vsiz);
        if (vbuf == nullptr) {
            return false;
        }
        const std::string raw = copy_tc(vbuf, vsiz);
        return key_storage_value_from_bytes(
            std::string_view(raw.data(), raw.size()), value);
    }

    void put_impl(const std::string &key, const ValueType &value) {
        if (!is_open_ || !db_) {
            return;
        }
        const std::string_view vb = key_storage_value_as_bytes(value);
        (void)tcbdbput(db_, key.data(), static_cast<int>(key.size()), vb.data(),
                       static_cast<int>(vb.size()));
    }

    bool get_or_insert_impl(const std::string &key,
                            const ValueType &value_to_insert,
                            ValueType &found_value) {
        if (get_impl(key, found_value)) {
            return true;
        }
        put_impl(key, value_to_insert);
        found_value = value_to_insert;
        return false;
    }

    void scan_impl(const std::string &key_start, size_t limit,
                   std::vector<std::pair<std::string, ValueType>> &results) {
        results.clear();
        if (!is_open_ || !db_ || limit == 0) {
            return;
        }
        auto it = lower_bound_impl(key_start);
        while (!it.is_end() && results.size() < limit) {
            results.emplace_back(it.get_key(), it.get_value());
            ++it;
        }
    }

    TokyoCabinetKeyStorageIterator<ValueType>
    lower_bound_impl(const std::string &key);

    bool is_open() const { return is_open_; }

    int64_t count() const {
        if (!is_open_ || !db_) {
            return 0;
        }
        return static_cast<int64_t>(tcbdbrnum(db_));
    }

    bool sync() {
        if (!is_open_ || !db_) {
            return false;
        }
        return tcbdbsync(db_);
    }

    void clear() {
        if (!is_open_ || !db_) {
            return;
        }
        (void)tcbdbvanish(db_);
    }

    bool remove(const std::string &key) {
        if (!is_open_ || !db_) {
            return false;
        }
        return tcbdbout(db_, key.data(), static_cast<int>(key.size()));
    }

    const std::string &get_path() const { return db_path_; }

    friend class TokyoCabinetKeyStorageIterator<ValueType>;
};

template <KeyStorageValueType ValueType> class TokyoCabinetKeyStorageIterator
    : public KeyStorageIterator<TokyoCabinetKeyStorageIterator<ValueType>,
                                ValueType> {
private:
    BDBCUR *cur_{nullptr};
    std::string current_key_;
    std::string current_value_;
    bool is_at_end_{true};

    void load_current() {
        int ksiz = 0;
        int vsiz = 0;
        void *kbuf = tcbdbcurkey(cur_, &ksiz);
        void *vbuf = tcbdbcurval(cur_, &vsiz);
        current_key_ = TokyoCabinetKeyStorage<ValueType>::copy_tc(kbuf, ksiz);
        current_value_ = TokyoCabinetKeyStorage<ValueType>::copy_tc(vbuf, vsiz);
        is_at_end_ = false;
    }

public:
    TokyoCabinetKeyStorageIterator(TCBDB *db, const std::string &key) {
        if (!db) {
            return;
        }
        cur_ = tcbdbcurnew(db);
        if (!cur_) {
            return;
        }
        bool ok;
        if (key.empty()) {
            ok = tcbdbcurfirst(cur_);
        } else {
            ok = tcbdbcurjump(cur_, key.data(), static_cast<int>(key.size()));
        }
        if (ok) {
            load_current();
        } else {
            tcbdbcurdel(cur_);
            cur_ = nullptr;
        }
    }

    ~TokyoCabinetKeyStorageIterator() {
        if (cur_) {
            tcbdbcurdel(cur_);
        }
    }

    TokyoCabinetKeyStorageIterator(const TokyoCabinetKeyStorageIterator &) =
        delete;
    TokyoCabinetKeyStorageIterator &
    operator=(const TokyoCabinetKeyStorageIterator &) = delete;

    TokyoCabinetKeyStorageIterator(
        TokyoCabinetKeyStorageIterator &&other) noexcept :
        cur_(other.cur_), current_key_(std::move(other.current_key_)),
        current_value_(std::move(other.current_value_)),
        is_at_end_(other.is_at_end_) {
        other.cur_ = nullptr;
        other.is_at_end_ = true;
    }

    TokyoCabinetKeyStorageIterator &
    operator=(TokyoCabinetKeyStorageIterator &&other) noexcept {
        if (this != &other) {
            if (cur_) {
                tcbdbcurdel(cur_);
            }
            cur_ = other.cur_;
            current_key_ = std::move(other.current_key_);
            current_value_ = std::move(other.current_value_);
            is_at_end_ = other.is_at_end_;
            other.cur_ = nullptr;
            other.is_at_end_ = true;
        }
        return *this;
    }

    std::string get_key_impl() const {
        return is_at_end_ ? std::string() : current_key_;
    }

    ValueType get_value_impl() const {
        if (is_at_end_) {
            return ValueType{};
        }
        ValueType out{};
        if (!key_storage_value_from_bytes(
                std::string_view(current_value_.data(), current_value_.size()),
                out)) {
            return ValueType{};
        }
        return out;
    }

    void increment_impl() {
        if (is_at_end_ || !cur_) {
            is_at_end_ = true;
            return;
        }
        if (tcbdbcurnext(cur_)) {
            load_current();
        } else {
            tcbdbcurdel(cur_);
            cur_ = nullptr;
            is_at_end_ = true;
        }
    }

    bool is_end_impl() const { return is_at_end_; }
};

template <KeyStorageValueType ValueType>
TokyoCabinetKeyStorageIterator<ValueType>
TokyoCabinetKeyStorage<ValueType>::lower_bound_impl(const std::string &key) {
    return TokyoCabinetKeyStorageIterator<ValueType>(db_, key);
}

template <KeyStorageValueType ValueType>
inline std::atomic_int TokyoCabinetKeyStorage<ValueType>::db_counter_ = 0;

template <KeyStorageValueType ValueType>
inline std::string TokyoCabinetKeyStorage<ValueType>::id_ = std::to_string(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count());
