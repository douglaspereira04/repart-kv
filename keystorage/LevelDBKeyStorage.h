#pragma once

#include "KeyStorage.h"
#include "KeyStorageIterator.h"
#include "KeyStorageValueBinary.h"
#include <leveldb/db.h>
#include <leveldb/options.h>
#include <leveldb/status.h>
#include <leveldb/write_batch.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

template <KeyStorageValueType ValueType> class LevelDBKeyStorageIterator;

/**
 * @brief LevelDB-based implementation of KeyStorage
 * @tparam ValueType The type of values stored (integral types or pointers)
 *
 * Persists string keys to lexicographic order; values are sizeof(ValueType)
 * raw bytes (see KeyStorageValueBinary).
 */
template <KeyStorageValueType ValueType> class LevelDBKeyStorage
    : public KeyStorage<LevelDBKeyStorage<ValueType>,
                        LevelDBKeyStorageIterator<ValueType>, ValueType> {
private:
    std::unique_ptr<leveldb::DB> db_;
    bool is_open_;
    std::string db_path_;

    static std::atomic_int db_counter_;
    static std::string id_;

    static leveldb::WriteOptions default_write_options() {
        leveldb::WriteOptions o;
        o.sync = false;
        return o;
    }

    void open_at_path(const std::string &path) {
        leveldb::Options options;
        options.create_if_missing = true;
        leveldb::DB *raw = nullptr;
        leveldb::Status status = leveldb::DB::Open(options, path, &raw);
        if (status.ok() && raw) {
            db_.reset(raw);
            is_open_ = true;
        } else {
            is_open_ = false;
        }
    }

public:
    /**
     * @param base_path Directory under which a unique \c repart_kv_keystorage
     *        subdirectory is created for this database.
     */
    explicit LevelDBKeyStorage(const std::string &base_path) :
        db_(nullptr), is_open_(false) {
        db_path_ =
            (std::filesystem::path(base_path) / "repart_kv_keystorage" / id_ /
             std::to_string(
                 db_counter_.fetch_add(1, std::memory_order_relaxed)))
                .string();
        std::filesystem::create_directories(db_path_);
        open_at_path(db_path_);
    }

    ~LevelDBKeyStorage() {
        if (is_open_ && db_) {
            db_.reset();
            is_open_ = false;
            if (!db_path_.empty()) {
                std::error_code ec;
                std::filesystem::remove_all(db_path_, ec);
            }
        }
    }

    LevelDBKeyStorage(const LevelDBKeyStorage &) = delete;
    LevelDBKeyStorage &operator=(const LevelDBKeyStorage &) = delete;

    LevelDBKeyStorage(LevelDBKeyStorage &&other) noexcept :
        db_(std::move(other.db_)), is_open_(other.is_open_),
        db_path_(std::move(other.db_path_)) {
        other.is_open_ = false;
    }

    LevelDBKeyStorage &operator=(LevelDBKeyStorage &&other) noexcept {
        if (this != &other) {
            if (is_open_ && db_) {
                db_.reset();
            }
            db_ = std::move(other.db_);
            is_open_ = other.is_open_;
            db_path_ = std::move(other.db_path_);
            other.is_open_ = false;
        }
        return *this;
    }

    bool get_impl(const std::string &key, ValueType &value) const {
        if (!is_open_ || !db_) {
            return false;
        }
        std::string raw;
        leveldb::Status s = db_->Get(leveldb::ReadOptions(), key, &raw);
        if (!s.ok()) {
            return false;
        }
        return key_storage_value_from_bytes(
            std::string_view(raw.data(), raw.size()), value);
    }

    void put_impl(const std::string &key, const ValueType &value) {
        if (!is_open_ || !db_) {
            return;
        }
        const std::string_view vb = key_storage_value_as_bytes(value);
        const std::string value_str(vb.data(), vb.size());
        (void)db_->Put(default_write_options(), key, value_str);
    }

    bool get_or_insert_impl(const std::string &key,
                            const ValueType &value_to_insert,
                            ValueType &found_value) {
        if (!is_open_ || !db_) {
            found_value = value_to_insert;
            return false;
        }
        std::string existing;
        leveldb::Status s = db_->Get(leveldb::ReadOptions(), key, &existing);
        if (s.ok()) {
            if (key_storage_value_from_bytes(
                    std::string_view(existing.data(), existing.size()),
                    found_value)) {
                return true;
            }
            found_value = value_to_insert;
            return false;
        }
        const std::string_view vb = key_storage_value_as_bytes(value_to_insert);
        const std::string value_str(vb.data(), vb.size());
        s = db_->Put(default_write_options(), key, value_str);
        if (s.ok()) {
            found_value = value_to_insert;
            return false;
        }
        s = db_->Get(leveldb::ReadOptions(), key, &existing);
        if (s.ok() && key_storage_value_from_bytes(
                          std::string_view(existing.data(), existing.size()),
                          found_value)) {
            return true;
        }
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

    LevelDBKeyStorageIterator<ValueType>
    lower_bound_impl(const std::string &key);

    bool is_open() const { return is_open_; }

    int64_t count() const {
        if (!is_open_ || !db_) {
            return 0;
        }
        leveldb::ReadOptions ro;
        std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(ro));
        int64_t n = 0;
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            ++n;
        }
        return n;
    }

    bool sync() {
        if (!is_open_ || !db_) {
            return false;
        }
        return true;
    }

    void clear() {
        if (!is_open_ || !db_) {
            return;
        }
        leveldb::ReadOptions ro;
        std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(ro));
        leveldb::WriteBatch batch;
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            batch.Delete(it->key());
        }
        (void)db_->Write(default_write_options(), &batch);
    }

    bool remove(const std::string &key) {
        if (!is_open_ || !db_) {
            return false;
        }
        std::string ignored;
        if (!db_->Get(leveldb::ReadOptions(), key, &ignored).ok()) {
            return false;
        }
        return db_->Delete(default_write_options(), key).ok();
    }

    const std::string &get_path() const { return db_path_; }

private:
};

template <KeyStorageValueType ValueType> class LevelDBKeyStorageIterator
    : public KeyStorageIterator<LevelDBKeyStorageIterator<ValueType>,
                                ValueType> {
private:
    leveldb::DB *db_;
    std::unique_ptr<leveldb::Iterator> iter_{};
    bool is_at_end_{true};

public:
    LevelDBKeyStorageIterator(leveldb::DB *db, const std::string &key) :
        db_(db) {
        if (!db_) {
            return;
        }
        iter_ = std::unique_ptr<leveldb::Iterator>(
            db->NewIterator(leveldb::ReadOptions()));
        if (key.empty()) {
            iter_->SeekToFirst();
        } else {
            iter_->Seek(key);
        }
        is_at_end_ = !iter_->Valid();
    }

    ~LevelDBKeyStorageIterator() = default;

    LevelDBKeyStorageIterator(const LevelDBKeyStorageIterator &) = delete;
    LevelDBKeyStorageIterator &
    operator=(const LevelDBKeyStorageIterator &) = delete;

    LevelDBKeyStorageIterator(LevelDBKeyStorageIterator &&other) noexcept :
        db_(other.db_), iter_(std::move(other.iter_)),
        is_at_end_(other.is_at_end_) {
        other.is_at_end_ = true;
    }

    LevelDBKeyStorageIterator &
    operator=(LevelDBKeyStorageIterator &&other) noexcept {
        if (this != &other) {
            db_ = other.db_;
            iter_ = std::move(other.iter_);
            is_at_end_ = other.is_at_end_;
            other.is_at_end_ = true;
        }
        return *this;
    }

    std::string get_key_impl() const {
        if (is_at_end_ || !iter_ || !iter_->Valid()) {
            return "";
        }
        return iter_->key().ToString();
    }

    ValueType get_value_impl() const {
        if (is_at_end_ || !iter_ || !iter_->Valid()) {
            return ValueType{};
        }
        const std::string_view stored(iter_->value().data(),
                                      iter_->value().size());
        ValueType out{};
        if (!key_storage_value_from_bytes(stored, out)) {
            return ValueType{};
        }
        return out;
    }

    void increment_impl() {
        if (is_at_end_ || !iter_ || !iter_->Valid()) {
            is_at_end_ = true;
            return;
        }
        iter_->Next();
        is_at_end_ = !iter_->Valid();
    }

    bool is_end_impl() const { return is_at_end_; }
};

template <KeyStorageValueType ValueType> LevelDBKeyStorageIterator<ValueType>
LevelDBKeyStorage<ValueType>::lower_bound_impl(const std::string &key) {
    return LevelDBKeyStorageIterator<ValueType>(db_.get(), key);
}

template <KeyStorageValueType ValueType>
inline std::atomic_int LevelDBKeyStorage<ValueType>::db_counter_ = 0;

template <KeyStorageValueType ValueType>
inline std::string LevelDBKeyStorage<ValueType>::id_ = std::to_string(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count());
