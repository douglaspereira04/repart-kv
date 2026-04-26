#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../../storage/MapStorageEngine.h"
#include "../../utils/test_resources.h"
#include "../HardRepartitioningKeyValueStorage.h"
#include "../LockStrippingKeyValueStorage.h"
#include "../SoftRepartitioningKeyValueStorage.h"
#include "../threaded/HardThreadedRepartitioningKeyValueStorage.h"
#include "../threaded/SoftThreadedRepartitioningKeyValueStorage.h"

namespace repart_kv_test {

[[nodiscard]] inline std::vector<std::string> partitioned_kv_test_paths() {
    return {test_resources_dir()};
}

namespace detail {

template <typename T> struct PartitionedStorageMaker;

template <bool S, template <typename> class K> struct PartitionedStorageMaker<
    HardRepartitioningKeyValueStorage<MapStorageEngine, S, K>> {
    static HardRepartitioningKeyValueStorage<MapStorageEngine, S, K>
    make(size_t partition_count) {
        return HardRepartitioningKeyValueStorage<MapStorageEngine, S, K>(
            partition_count, std::hash<std::string>{}, std::nullopt,
            std::nullopt, partitioned_kv_test_paths());
    }
};

template <bool S, template <typename> class K> struct PartitionedStorageMaker<
    SoftRepartitioningKeyValueStorage<MapStorageEngine, S, K, K>> {
    static SoftRepartitioningKeyValueStorage<MapStorageEngine, S, K, K>
    make(size_t partition_count) {
        return SoftRepartitioningKeyValueStorage<MapStorageEngine, S, K, K>(
            partition_count, std::hash<std::string>{}, std::nullopt,
            std::nullopt, partitioned_kv_test_paths());
    }
};

template <template <bool> class Eng, bool S, template <typename> class K,
          typename HashFunc, size_t Q>
struct PartitionedStorageMaker<
    SoftThreadedRepartitioningKeyValueStorage<Eng, S, K, HashFunc, Q>> {
    static SoftThreadedRepartitioningKeyValueStorage<Eng, S, K, HashFunc, Q>
    make(size_t partition_count) {
        return SoftThreadedRepartitioningKeyValueStorage<Eng, S, K, HashFunc,
                                                         Q>(
            partition_count, std::hash<std::string>{}, std::nullopt,
            std::nullopt, partitioned_kv_test_paths());
    }
};

template <template <bool> class Eng, bool S, template <typename> class KM,
          template <typename> class KP, typename HashFunc, size_t Q>
struct PartitionedStorageMaker<
    HardThreadedRepartitioningKeyValueStorage<Eng, S, KM, KP, HashFunc, Q>> {
    static HardThreadedRepartitioningKeyValueStorage<Eng, S, KM, KP, HashFunc,
                                                     Q>
    make(size_t partition_count) {
        return HardThreadedRepartitioningKeyValueStorage<Eng, S, KM, KP,
                                                         HashFunc, Q>(
            partition_count, std::hash<std::string>{}, std::nullopt,
            std::nullopt, partitioned_kv_test_paths());
    }
};

template <bool S> struct PartitionedStorageMaker<
    LockStrippingKeyValueStorage<MapStorageEngine, S>> {
    static LockStrippingKeyValueStorage<MapStorageEngine, S>
    make(size_t partition_count) {
        return LockStrippingKeyValueStorage<MapStorageEngine, S>(
            partition_count, std::hash<std::string>{},
            partitioned_kv_test_paths());
    }
};

} // namespace detail

template <typename StorageType>
StorageType make_partitioned_kv_test_storage(size_t partition_count) {
    return detail::PartitionedStorageMaker<StorageType>::make(partition_count);
}

} // namespace repart_kv_test
