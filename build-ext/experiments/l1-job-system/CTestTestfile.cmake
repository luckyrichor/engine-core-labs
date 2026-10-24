# CMake generated Testfile for 
# Source directory: /home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system
# Build directory: /home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l1-job-system
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[owner_queue]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l1-job-system/queue_test")
set_tests_properties([=[owner_queue]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;6;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;0;")
add_test([=[workload_contract]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l1-job-system/workload_test")
set_tests_properties([=[workload_contract]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;10;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;0;")
add_test([=[pool_lifecycle]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l1-job-system/pool_test")
set_tests_properties([=[pool_lifecycle]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;19;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;0;")
add_test([=[steal_contract]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l1-job-system/steal_test")
set_tests_properties([=[steal_contract]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;20;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;0;")
add_test([=[benchmark_baseline_smoke]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l1-job-system/job_benchmark" "baseline" "2" "100" "100" "balanced")
set_tests_properties([=[benchmark_baseline_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;21;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;0;")
add_test([=[benchmark_stealing_smoke]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l1-job-system/job_benchmark" "stealing" "2" "100" "100" "skewed" "prequeued")
set_tests_properties([=[benchmark_stealing_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;22;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;0;")
add_test([=[workload_shape]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l1-job-system/shape_test")
set_tests_properties([=[workload_shape]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;26;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l1-job-system/CMakeLists.txt;0;")
