# CMake generated Testfile for 
# Source directory: /home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l2-allocators
# Build directory: /home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l2-allocators
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[allocator_boundaries]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l2-allocators/allocator_tests")
set_tests_properties([=[allocator_boundaries]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l2-allocators/CMakeLists.txt;6;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l2-allocators/CMakeLists.txt;0;")
add_test([=[variable_fragmentation]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l2-allocators/fragmentation_demo")
set_tests_properties([=[variable_fragmentation]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l2-allocators/CMakeLists.txt;9;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l2-allocators/CMakeLists.txt;0;")
add_test([=[cold_payload]=] "/home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/experiments/l2-allocators/allocator_cold" "prefaulted")
set_tests_properties([=[cold_payload]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l2-allocators/CMakeLists.txt;17;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/experiments/l2-allocators/CMakeLists.txt;0;")
