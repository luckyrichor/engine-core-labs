# CMake generated Testfile for 
# Source directory: /home/ubuntu/project/WorkPlan/engine-core-labs/tools/bistro-aabb
# Build directory: /home/ubuntu/project/WorkPlan/engine-core-labs/build-ext/tools/bistro-aabb
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[bench_aabb_reference]=] "/usr/bin/python3" "/home/ubuntu/project/WorkPlan/engine-core-labs/tools/bistro-aabb/test_reference.py" "/home/ubuntu/data/engine-core-labs/tools/bin/aabb_reference")
set_tests_properties([=[bench_aabb_reference]=] PROPERTIES  WORKING_DIRECTORY "/home/ubuntu/project/WorkPlan/engine-core-labs" _BACKTRACE_TRIPLES "/home/ubuntu/project/WorkPlan/engine-core-labs/tools/bistro-aabb/CMakeLists.txt;17;add_test;/home/ubuntu/project/WorkPlan/engine-core-labs/tools/bistro-aabb/CMakeLists.txt;0;")
