# C++ template class PrivateAllocator<>

Private (per container instance) STL Allocator

The template class PrivateAllocator<> is indended to be used by container-aware STL 
containers in C++11 (and later) as 'allocator' template argument.

Unlike std::allocator<> (which is the default allocator argument for all STL containers), 
it keeps private memory-pool bound to the particular container instances. It allows for 
more efficient (faster) memory allocation and deallocation as well as for smaller memory 
footprint. 

This allocation scheme is potentially beneficial in scenarios involving multiple 
allocations of small blocks of the same size. In practice, this means the usage of 
the STL node-based containers: 
- forward_list<>
- list<>
- set<> and multiset<> 
- map<> and multimap<>
- unordered_set<> and unordered_multiset<>
- unordered_map<> and unordered_multimap<>

The implementation involves allocating of "pages" - contigous blocks of memory - of 
exponentially-increasing sizes, dividing these internally to same-size blocks, 
and maintaining of a free list of these blocks. 

Once allocated, the pages and blocks are only released upon the allocator destruction.

The potential benefit (as compared to std::allocator<>) comes from:
- performing fewer, larger-block allocation from the external allocator; 
  (::operator new() and operator::delete()), thereby reducing the memory footprint;
- performing fast, lock-free allocation/deallocation by using the private free-block
  list;
- faster data access due to placement of the user data in (mostly) contigous memory.

The incured higher costs may come from:
- the need to maintain state which increases the size of the client containers 
  (currenly by 2*sizof(void*));
- potential higher memory usage for 'small' (1-2 items) containers, due to the
  overhead of the Page-allocation system;
- the memory that the container requested is not freed until the container gets 
  destroyed. That would lead to larger memory footprint if the container deletes
  significant count of items compared to the high-water mark.

The source code consists of:
  PrivateAllocator.h, PrivateAllocator.cpp
    - definition of template class PrivateAllocator
  PageAllocator.h, PageAllocator.cpp 
    - non-template classes and functions supporting the implementation of 
      PravateAllocator<>. 
  BackendAllocators.h, BackendAllocators.cpp 
    - provide the 'back-end' allocation needed by the above
  Unittest.h, Unittest.cpp
    - small ad-hoc unittest framework
  Benchmarks.cpp
    - benchmarks for std::allocator and PrivateAllocator for 
      several standard containers
  Makefile
      makefile for building the benchmark suite, for Linux and OSX

To use the allocator in your code, you should include *all* of the source files
mentioned above, except for Benchmarks.cpp. Note that the unittests will still be 
compiled when you build the code but they would only be invoked if you choose to 
explicitely do it.

To use the allocator in your container, put it as parameter instead of the (default) 
std::allocator. For instance:

#include "PrivateAllocator"
...
std::list<int, rg_privateallocator::PrivateAllocator<int>> myList;
...
   
To assess the actual allocator performance, a series of benchmarks is provided. The 
benchmarks measure the performance (speed) of several standard containers 
(vector/list/set etc), paired with both std::allocator<> and PrivateAllocator<>, 
excercising several dirrerent algorithms (fill/copy/insertDelete etc).

The pre-built benchmarks for several platforms and compilers (Windows/OSX/Linux),
along with the results of these benchmarks are also provided, in the respectively-named
subdirectories.

One of the most-representative benchmark is the copy of std::forward_list<>, due 
to the fact that it is the node-based std allocator with the smallest intrinsic 
overhead (it is essentially a simple linked lists. Here are some results, with variations 
for the different platforms:
- PrivateAllocator<> uses between 1x and 3x less memory than std::allocator<>; 
- For 1-thread benchmark, PrivateAllocator<> is between and 2x and 9x times faster;
- For 4-thread benchmark, PrivateAllocator<> is between and 3x and 20x times faster.
Most of the remaining benchmarks also show advantages of PrivateAllocator<>.

Some relevant additional info can be found here: 
  https://en.cppreference.com/w/cpp/named_req/Allocator
  https://foonathan.net/2015/10/allocatorawarecontainer-propagation-pitfalls/
  https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer

Author: 
    Radoslav Getov, getov@mail.com  
