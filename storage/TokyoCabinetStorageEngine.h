#pragma once

#include "StorageEngineIterator.h"
#include "StorageEngine.h"
#include <tcbdb.h>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <utility>

/**
 * @brief Tokyo Cabinet B+tree StorageEngine without internal mutexes.
 *
 * Uses \c TCBDB and never calls \c tcbdbsetmutex (the B+tree analog of
 * \c tchdbsetmutex). Thread safety is caller-owned: a reader-writer lock
 * around the instance is sufficient for parallel reads vs exclusive writes.
 *
 * @tparam SYNC When true, \c tcbdbsync after mutating writes.
 */
template <bool SYNC = false> class TokyoCabinetStorageEngine
    : public StorageEngine<TokyoCabinetStorageEngine<SYNC>, SYNC> {
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
        // BDBTLARGE: files >2GiB (engine mode of experiment_set_large is 32GiB).
        // lmemb=32 suits ~4KiB values; bnum sized for ~8M pages.
        if (!tcbdbtune(db_, 32, 256, 8388608, -1, -1, BDBTLARGE)) {
            tcbdbdel(db_);
            db_ = nullptr;
            return false;
        }
        const int omode = BDBOWRITER | BDBOCREAT;
        if (!tcbdbopen(db_, file_path.c_str(), omode)) {
            tcbdbdel(db_);
            db_ = nullptr;
            return false;
        }
        db_path_ = file_path;
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

    Status sync_if_needed() {
        if constexpr (SYNC) {
            if (!tcbdbsync(db_)) {
                return Status::ERROR;
            }
        }
        return Status::SUCCESS;
    }

    Status
    cursor_scan(const std::string &start_key, size_t limit,
                std::vector<std::pair<std::string, std::string>> &results,
                bool clear_results) const {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        if (clear_results) {
            results.clear();
        }
        if (limit == 0) {
            return Status::NOT_FOUND;
        }
        BDBCUR *cur = tcbdbcurnew(db_);
        if (cur == nullptr) {
            return Status::ERROR;
        }
        bool ok;
        if (start_key.empty()) {
            ok = tcbdbcurfirst(cur);
        } else {
            ok = tcbdbcurjump(cur, start_key.data(),
                              static_cast<int>(start_key.size()));
        }
        size_t count = 0;
        while (ok && count < limit) {
            int ksiz = 0;
            int vsiz = 0;
            void *kbuf = tcbdbcurkey(cur, &ksiz);
            void *vbuf = tcbdbcurval(cur, &vsiz);
            results.emplace_back(copy_tc(kbuf, ksiz), copy_tc(vbuf, vsiz));
            ++count;
            ok = tcbdbcurnext(cur);
        }
        tcbdbcurdel(cur);
        if (count == 0) {
            return Status::NOT_FOUND;
        }
        return Status::SUCCESS;
    }

public:
    explicit TokyoCabinetStorageEngine(size_t level = 0,
                                       const std::string &path = "/tmp") :
        StorageEngine<TokyoCabinetStorageEngine<SYNC>, SYNC>(level, path),
        db_(nullptr), is_open_(false) {
        const std::string file_path =
            this->path_ + std::string("/repart_kv_storage/") + id_ +
            std::string("/tc_temp_") +
            std::to_string(
                db_counter_.fetch_add(1, std::memory_order_relaxed)) +
            ".tcb";
        std::filesystem::create_directories(
            this->path_ + std::string("/repart_kv_storage/") + id_);
        (void)open_at_path(file_path);
    }

    explicit TokyoCabinetStorageEngine(const std::string &file_path,
                                       size_t level = 0,
                                       const std::string &path = "/tmp") :
        StorageEngine<TokyoCabinetStorageEngine<SYNC>, SYNC>(level, path),
        db_(nullptr), is_open_(false), db_path_(file_path) {
        (void)open_at_path(file_path);
    }

    ~TokyoCabinetStorageEngine() { close_handles(); }

    TokyoCabinetStorageEngine(const TokyoCabinetStorageEngine &) = delete;
    TokyoCabinetStorageEngine &
    operator=(const TokyoCabinetStorageEngine &) = delete;

    TokyoCabinetStorageEngine(TokyoCabinetStorageEngine &&other) noexcept :
        StorageEngine<TokyoCabinetStorageEngine<SYNC>, SYNC>(other.level_,
                                                             other.path_),
        db_(other.db_), is_open_(other.is_open_),
        db_path_(std::move(other.db_path_)) {
        other.db_ = nullptr;
        other.is_open_ = false;
    }

    TokyoCabinetStorageEngine &
    operator=(TokyoCabinetStorageEngine &&other) noexcept {
        if (this != &other) {
            close_handles();
            db_ = other.db_;
            is_open_ = other.is_open_;
            db_path_ = std::move(other.db_path_);
            other.db_ = nullptr;
            other.is_open_ = false;
        }
        return *this;
    }

    Status read_impl(const std::string &key, std::string &value) const {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        int vsiz = 0;
        void *vbuf =
            tcbdbget(db_, key.data(), static_cast<int>(key.size()), &vsiz);
        if (vbuf == nullptr) {
            return Status::NOT_FOUND;
        }
        value = copy_tc(vbuf, vsiz);
        return Status::SUCCESS;
    }

    Status write_impl(const std::string &key, const std::string &value) {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        if (!tcbdbput(db_, key.data(), static_cast<int>(key.size()),
                      value.data(), static_cast<int>(value.size()))) {
            return Status::ERROR;
        }
        return sync_if_needed();
    }

    Status async_write_impl(const std::string &key, const std::string &value) {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        if (!tcbdbput(db_, key.data(), static_cast<int>(key.size()),
                      value.data(), static_cast<int>(value.size()))) {
            return Status::ERROR;
        }
        return Status::SUCCESS;
    }

    Status force_sync_impl() {
        if (!is_open_ || !db_) {
            return Status::ERROR;
        }
        return tcbdbsync(db_) ? Status::SUCCESS : Status::ERROR;
    }

    Status
    scan_impl(const std::string &initial_key_prefix, size_t limit,
              std::vector<std::pair<std::string, std::string>> &results) const {
        return cursor_scan(initial_key_prefix, limit, results, true);
    }

    Status
    scan_append(const std::string &key_start, size_t limit,
                std::vector<std::pair<std::string, std::string>> &results) {
        return cursor_scan(key_start, limit, results, false);
    }

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
        if constexpr (SYNC) {
            return tcbdbsync(db_);
        }
        return true;
    }

    void clear() {
        if (!is_open_ || !db_) {
            return;
        }
        (void)tcbdbvanish(db_);
        (void)sync_if_needed();
    }

    Status remove_impl(const std::string &key, std::string &removed_value) {
        const Status st = read_impl(key, removed_value);
        if (st != Status::SUCCESS) {
            return st;
        }
        if (!tcbdbout(db_, key.data(), static_cast<int>(key.size()))) {
            return Status::ERROR;
        }
        return sync_if_needed();
    }

    class TokyoCabinetIterator
        : public StorageEngineIterator<TokyoCabinetIterator,
                                       TokyoCabinetStorageEngine<SYNC>> {
    private:
        BDBCUR *cur_;

    public:
        explicit TokyoCabinetIterator(TokyoCabinetStorageEngine &engine) :
            StorageEngineIterator<TokyoCabinetIterator,
                                  TokyoCabinetStorageEngine<SYNC>>(engine),
            cur_(nullptr) {
            if (engine.is_open_ && engine.db_) {
                cur_ = tcbdbcurnew(engine.db_);
            }
        }

        TokyoCabinetIterator(const TokyoCabinetIterator &) = delete;
        TokyoCabinetIterator &operator=(const TokyoCabinetIterator &) = delete;

        TokyoCabinetIterator(TokyoCabinetIterator &&other) noexcept :
            StorageEngineIterator<TokyoCabinetIterator,
                                  TokyoCabinetStorageEngine<SYNC>>(
                *other.engine_),
            cur_(other.cur_) {
            other.cur_ = nullptr;
        }

        TokyoCabinetIterator &operator=(TokyoCabinetIterator &&other) noexcept {
            if (this != &other) {
                if (cur_) {
                    tcbdbcurdel(cur_);
                }
                this->engine_ = other.engine_;
                cur_ = other.cur_;
                other.cur_ = nullptr;
            }
            return *this;
        }

        ~TokyoCabinetIterator() {
            if (cur_) {
                tcbdbcurdel(cur_);
                cur_ = nullptr;
            }
        }

        Status find_impl(const std::string &key, std::string &value) {
            if (!cur_ || !this->engine_->is_open_) {
                return Status::ERROR;
            }
            if (!tcbdbcurjump(cur_, key.data(), static_cast<int>(key.size()))) {
                return Status::NOT_FOUND;
            }
            int ksiz = 0;
            void *kbuf = tcbdbcurkey(cur_, &ksiz);
            std::string found = copy_tc(kbuf, ksiz);
            if (found != key) {
                return Status::NOT_FOUND;
            }
            int vsiz = 0;
            void *vbuf = tcbdbcurval(cur_, &vsiz);
            value = copy_tc(vbuf, vsiz);
            return Status::SUCCESS;
        }
    };

    TokyoCabinetIterator iterator_impl() { return TokyoCabinetIterator(*this); }

    using IteratorType = TokyoCabinetIterator;
};

template <bool SYNC>
std::atomic_int TokyoCabinetStorageEngine<SYNC>::db_counter_ = 0;
template <bool SYNC> std::string TokyoCabinetStorageEngine<SYNC>::id_ =
    std::to_string(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count());
