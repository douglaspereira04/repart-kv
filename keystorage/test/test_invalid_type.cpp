#include "MapKeyStorage.h"
#include "../../utils/test_resources.h"

int main() {
    // This should fail to compile - double is not integral or pointer
    MapKeyStorage<double> doubleStorage(repart_kv_test::test_resources_dir());
    return 0;
}
