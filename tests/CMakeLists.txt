cmake_minimum_required(VERSION 3.18)
project(ofxGgmlRag_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ofxGgmlRag_tests
	test_main.cpp
	../src/ofxGgmlRag/ofxGgmlRag.cpp
	../src/ofxGgmlRag/ofxGgmlRagUtils.cpp)

target_include_directories(ofxGgmlRag_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../src)

enable_testing()
add_test(NAME ofxGgmlRag_tests COMMAND ofxGgmlRag_tests)
