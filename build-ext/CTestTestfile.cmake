# CMake generated Testfile for 
# Source directory: /home/ubuntu/project/WorkPlan/engine-core-labs
# Build directory: /home/ubuntu/project/WorkPlan/engine-core-labs/build-ext
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[bench_unittest]=] "/usr/bin/python3" "-m" "unittest" "discover" "-s" "bench/tests" "-v")
set_tests_properties([=[bench_unittest]=] PROPERTIES  WORKING_DIRECTORY "/home/ubuntu/project/WorkPlan/engine-core-labs" _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/CMakeLists.txt;21;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/CMakeLists.txt;0;")
add_test([=[bench_workload_cpp]=] "/usr/bin/python3" "-m" "bench.tests.test_workload_cpp" "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/workload_reader_test")
set_tests_properties([=[bench_workload_cpp]=] PROPERTIES  WORKING_DIRECTORY "/home/ubuntu/project/WorkPlan/engine-core-labs" _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/CMakeLists.txt;27;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/CMakeLists.txt;0;")
subdirs("experiments/l1-job-system")
subdirs("experiments/l2-allocators")
subdirs("experiments/l3-resource-loading")
subdirs("tools/bistro-aabb")
