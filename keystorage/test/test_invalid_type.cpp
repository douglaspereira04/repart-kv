#include "MapKeyStorage.h"

int main() {
    // This should fail to compile - double is not integral or pointer
    MapKeyStorage<double> doubleStorage;
    return 0;
}
