#include "workload.hpp"
#include <stdexcept>
int main() {
    using engine_labs::workload;
    if (workload(0, 0) != 1 || workload(0, 100) != workload(0, 100))
        throw std::runtime_error("non reproducible workload");
    if (workload(0, 100) == workload(1, 100)) throw std::runtime_error("identical task loads");
}
