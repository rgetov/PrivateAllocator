# Private (per container instance) C++ STL Allocator.
# Makefile for Linux/OSX platforms.
# Builds executable capable of running unittests and benchmark suites
# Author: 
#   Radoslav Getov, getov@mail.com  

RunBenchmarks.exe :  Makefile Benchmarks.cpp                   \
                     BackendAllocators.cpp BackendAllocators.h \
                     Unittest.h Unittest.cpp                   \
                     PageAllocator.cpp PageAllocator.h         \
                     PrivateAllocator.h PrivateAllocator.cpp 
	g++ -std=c++11 -DNDEBUG -m64 -O3 -o RunBenchmarks.exe \
      Benchmarks.cpp Unittest.cpp BackendAllocators.cpp \
      PageAllocator.cpp PrivateAllocator.cpp -pthread


