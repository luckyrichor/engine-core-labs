#include "../scheduler_adapter.hpp"
#include "../workload_io.hpp"
#include <iostream>
#include <numeric>
int main(int argc, char** argv) {
    using namespace ecl::workload;
    try {
        require(argc == 4, "usage: scheduler_check impl threads meta.json");
        const auto meta = verify_meta(argv[3]);
        require(meta.at("kind").str() == "task_costs", "expected task_costs");
        require(meta.at("files").object.size() == 1, "expected one task file");
        const auto costs = read_task_costs(verified_file(argv[3], meta, meta.at("files").object.begin()->first));
        require(costs.size() == meta.at("answer").at("task_count").u64(), "task count mismatch");
        const auto threads = std::stoull(argv[2]);
        const auto result = ecl::adapter::execute(argv[1], costs, threads);
        require(result.checksum == meta.at("answer").at("cost_sum").u64(), "checksum mismatch");
        for (std::size_t i=0; i<costs.size(); ++i) {
            require(result.executions[i] == 1, "task not executed exactly once");
            std::uint64_t digest=i+1;
            for (std::uint32_t k=0; k<costs[i]; ++k) digest=(digest^(digest>>7))*0x9e3779b97f4a7c15ULL+k;
            require(result.work_digests[i] == digest, "work digest mismatch");
        }
        std::cout << "{\"tasks\":" << costs.size() << ",\"checksum\":" << result.checksum
                  << ",\"exactly_once\":true,\"work_verified\":true}\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
