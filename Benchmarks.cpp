// Benchmarks.cpp
//
// Benchmark of combinations of several C++ STL containers selectively paired with\n
// std::allocator<> and rg::PrivateAllocator<>. 
// Measures performance (speed) and allows memory usage assessment by using 
// external, per process tools.\n
//
// Author: 
//    Radoslav Getov, getov@mail.com  


// ------------------------------------- #Includes ---------------------------------------

// No own header file

#include "PrivateAllocator.h"
#include "Unittest.h"

#include <string>
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <chrono>
#include <thread>
#include <numeric>
#include <iterator>
#include <algorithm>
#include <cfloat>

// ------------------------------------- Definitions -------------------------------------

namespace rg_privateallocator
{ 

// The 'value' type in containers for all benchmark.
typedef long long BenchmarkValue;

// The PrivateAllocator for the benchmarks
typedef PrivateAllocator<BenchmarkValue> PA_Type;

// The std containers being benchmarked, with alternative allocators:
typedef std::vector<BenchmarkValue, PA_Type> PA_vector;
typedef std::vector<BenchmarkValue> vector;

typedef std::deque<BenchmarkValue, PA_Type> PA_deque;
typedef std::deque<BenchmarkValue> deque;

typedef std::list<BenchmarkValue, PA_Type> PA_list;
typedef std::list<BenchmarkValue> list;

typedef std::forward_list<BenchmarkValue, PA_Type> PA_forward_list;
typedef std::forward_list<BenchmarkValue> forward_list;

typedef std::multiset<BenchmarkValue, std::less<BenchmarkValue>, PA_Type> PA_multiset;
typedef std::multiset<BenchmarkValue> multiset;

typedef std::unordered_multiset<BenchmarkValue, 
                                std::hash<BenchmarkValue>, 
                                std::equal_to<BenchmarkValue>, 
                                PA_Type> PA_hash;
typedef std::unordered_multiset<BenchmarkValue> hash;

// The 'pure' (data only) memory for each benchmark - per thread, in bytes. 
const size_t cnBenchmarkMemory = sizeof(void*) >= 8 // i.e. 64bit platform
                                   ? 100*1000*1000 
                                   : 50*1000*1000; // 100MB too much for 32-bit

// The peak sum of the sizes of containers during a benchmark
const size_t cnBenchmarkCapacity = cnBenchmarkMemory / sizeof(BenchmarkValue);

// The minimal duration of each single benchmark, in seconds.
double dBenchmarkDuration = 5.0; 

// The signature of a benchmark-performing function.
// Should set '*result' with the result of its execution, the operations per second.
typedef void (*BenchmarkingFunction) (double* outputResultCallsPerSecond);


//========================================================================================
// Executes 'function' in 'nThreadCount' threads.
// Returns the sum of all the returns from 'function' (rough '*ret' param).
//________________________________________________________________________________________
static double runInParallelAndAccumulate(BenchmarkingFunction function, int nThreadCount)
{
  std::vector<std::thread> threads(nThreadCount);
  std::vector<double> results(nThreadCount);

  for (int j = 0; j < nThreadCount; ++j)
    threads[j] = std::thread(function, &results[j]);
  for (int j = 0; j < nThreadCount; ++j)
    threads[j].join();

  double ret = std::accumulate(results.begin(), results.end(), 0.0);
  return ret;
}

//========================================================================================
// Benchmarks two (presumably related, assuming using private snd standard allocators) 
// functions and prints their results and ratio thereof.
//________________________________________________________________________________________
static void benchmarkSideBySide(BenchmarkingFunction function_PA, 
                                BenchmarkingFunction function_STD, 
                                int                  nThreadCount)
{
  std::cout << "   " << nThreadCount << " thread(s): ";
  double pa = runInParallelAndAccumulate(function_PA, nThreadCount);
  std::cout << "  private = " << pa << "/sec; " << std::flush;
  double std = runInParallelAndAccumulate(function_STD, nThreadCount);
  std::cout << "std = " << std << "/sec (ratio = " << pa/std << ")" << std::endl;
}

//========================================================================================
// Helper function.
// Fill 'container' up to 'size' by inserting items to front or back,
// depending on the container type.
//________________________________________________________________________________________
template <typename Container>
static void fillContainer(Container& container, size_t size)
{
  for (size_t j = 0; j < size; ++j) 
    container.insert(container.end(), j);
}

// Specialization for forward_list that inserts to front.
template <typename V, typename A>
static void fillContainer(std::forward_list<V, A>& container, size_t size)
{
  for (size_t j = 0; j < size; ++j) 
    container.push_front(j);
}


//========================================================================================
// Returns current wall-clock time relative to some arbitrary initial moment (~1st call).
//________________________________________________________________________________________
static double getWallclockTime()
{
  using hrc = std::chrono::high_resolution_clock;
  static auto overalStart = hrc::now();

  typedef std::chrono::duration<double> Duration; // in seconds
  Duration d = hrc::now() - overalStart;
  double seconds = d.count();
  return seconds;
}

//========================================================================================
// Creates a local Container of size 'preFillSize' and then measures the average 
// call rate to 'measuredFunction(container)' in calls per second.
// Writes the result to '*outputResultCallsPerSecond'.
//________________________________________________________________________________________
template <typename Container>
void measureContainerFunctionCallRate(void (*measuredFunction)(Container&), 
                                      size_t preFillSize, 
                                      double *outputResultCallsPerSecond)
{
  // Pre-fill a local container:
  Container localContainer;
  fillContainer(localContainer, preFillSize);

  // Regardless of overal duration perform at least that many loops:
  const int cnMinLoops 
    #ifdef NDEBUG
      = 5;
    #else
      = 1;
    #endif  
  double startTime = getWallclockTime();
  for (int n = 0; /**/; ++n)
  {
    double ellapsedTime = getWallclockTime() - startTime;
    if (n >= cnMinLoops && ellapsedTime > dBenchmarkDuration) 
    {
      double aveTimePerCall = ellapsedTime / n;
      *outputResultCallsPerSecond = 1.0 / aveTimePerCall; // Ave calls per second
      return;
    }
    
    // ***** BEGIN THE CODE BEING MEASURED ******
    measuredFunction(localContainer);
    // ***** END THE CODE BEING MEASURED *****
  }
}

//========================================================================================
// Benchmarks 'container fill' operation.
//________________________________________________________________________________________
template <typename Container>
static void benchmarkFill(double* outputResultCallsPerSecond)
{
  auto testFunction = [](Container&)->void
  {
    Container local;
    fillContainer(local, cnBenchmarkCapacity);
  };
  measureContainerFunctionCallRate<Container>(testFunction, 
                                              0, // Don't need pre-filled container.
                                              outputResultCallsPerSecond);
}

//========================================================================================
//________________________________________________________________________________________
static void doAllFillBenchmarks()
{
  std::cout << "**************** Side by side benchmarks - FILL: ****************\n";
  
  std::cout << "vector<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkFill<PA_vector>, benchmarkFill<vector>, tc);

  std::cout << "forward_list<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkFill<PA_forward_list>, benchmarkFill<forward_list>, tc);

  std::cout << "list<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkFill<PA_list>, benchmarkFill<list>, tc);

  std::cout << "multiset<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkFill<PA_multiset>, benchmarkFill<multiset>, tc);

  std::cout << "hash<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkFill<PA_hash>, benchmarkFill<hash>, tc);

  std::cout << '\n';
}

//========================================================================================
// Benchmarks 'container copy'. 
//________________________________________________________________________________________
template <typename Container>
static void benchmarkCopy(double* outputResultCallsPerSecond)
{
  auto copyContainer = [](Container& container)->void
  {
    Container copy = container;
    std::swap(copy, container);
  };
  measureContainerFunctionCallRate<Container>(copyContainer, 
                                              cnBenchmarkCapacity/2, // *half* due to copy
                                              outputResultCallsPerSecond);
}

//========================================================================================
//________________________________________________________________________________________
static void doAllCopyBenchmarks()
{
  std::cout << "**************** Side by side benchmarks - COPY: ****************\n";
  
  std::cout << "vector<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkCopy<PA_vector>, benchmarkCopy<vector>, tc);

  std::cout << "forward_list<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkCopy<PA_forward_list>, benchmarkCopy<forward_list>, tc);

  std::cout << "list<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkCopy<PA_list>, benchmarkCopy<list>, tc);

  std::cout << "multiset<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkCopy<PA_multiset>, benchmarkCopy<multiset>, tc);

  std::cout << "hash<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkCopy<PA_hash>, benchmarkCopy<hash>, tc);

  std::cout << '\n';
}

//========================================================================================
// Helper function.
// Insert/delete a single item in 'container', at sequential positions.
//________________________________________________________________________________________
template <typename Container>
static void insertDeleteInContainer(Container& container)
{
  for (auto i = container.begin(); i != container.end(); /**/)
  {
    auto v = *i;
    auto next = std::next(i);
    container.insert(i, v);
    container.erase(i);
    i = next;
  }
}

// Specialization for 'forward_list<>' that has different interface.
template <typename V, typename A>
static void insertDeleteInContainer(std::forward_list<V, A>& container)
{
  for (auto i = container.begin(); i != container.end(); /**/)
  {
    auto next = std::next(i);
    if (next == container.end())
      break;
    auto v = *next;
    container.erase_after(i);
    i = container.insert_after(i, v);
  }
}

//========================================================================================
// Benchmarks 'remove/re-insert all items' in a Container.
//________________________________________________________________________________________
template <typename Container>
static void benchmarkInsertDelete(double* outputResultCallsPerSecond)
{
  measureContainerFunctionCallRate<Container>(insertDeleteInContainer, 
                                              cnBenchmarkCapacity, 
                                              outputResultCallsPerSecond);
}

//========================================================================================
//________________________________________________________________________________________
static void doAllInsertDeleteBenchmarks()
{
  std::cout << "************** Side by side benchmarks - INSERT/DELETE: **************\n";
  
  std::cout << "forward_list<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkInsertDelete<PA_forward_list>, 
                        benchmarkInsertDelete<forward_list>, 
                        tc);

  std::cout << "list<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkInsertDelete<PA_list>, benchmarkInsertDelete<list>, tc);

  std::cout << "multiset<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkInsertDelete<PA_multiset>, 
                        benchmarkInsertDelete<multiset>, 
                        tc);

  std::cout << "hash<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkInsertDelete<PA_hash>, benchmarkInsertDelete<hash>, tc);

  std::cout << '\n';
}


//========================================================================================
// Read/modify/write all items in a Container.
//________________________________________________________________________________________
template <typename Container>
static void benchmarkReadWrite(double* outputResultCallsPerSecond)
{
  auto readModifyWrite = [](Container& container) -> void
  {
    // Read/write each item:
    for (auto& i : container)
      ++i; // Read/modify/write
    // Additional sequential full read, just for fun:
    container.front() += std::accumulate(container.begin(), 
                                         container.end(), 
                                         BenchmarkValue());    
  };
  measureContainerFunctionCallRate<Container>(readModifyWrite, 
                                              cnBenchmarkCapacity, 
                                              outputResultCallsPerSecond);
}

//========================================================================================
//________________________________________________________________________________________
static void doAllReadWriteBenchmarks()
{
  std::cout << "************** Side by side benchmarks - READ/WRITE: *************\n";

  std::cout << "vector<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkReadWrite<PA_vector>, benchmarkReadWrite<vector>, tc);

  std::cout << "forward_list<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkReadWrite<PA_forward_list>, 
                        benchmarkReadWrite<forward_list>, 
                        tc);

  std::cout << "list<>:\n";
  for (int tc : {1, 4})
    benchmarkSideBySide(benchmarkReadWrite<PA_list>, benchmarkReadWrite<list>, tc);

  std::cout << '\n';
}

//========================================================================================
//________________________________________________________________________________________
static void doAllSideBySideBenchmarks()
{
  doAllFillBenchmarks();
  doAllCopyBenchmarks(); 
  doAllInsertDeleteBenchmarks();
  doAllReadWriteBenchmarks();
}


//========================================================================================
// Select a test function given the names of the container- and algorithm-type,
// and the option to use the private or the std allocator<>.
//________________________________________________________________________________________
static BenchmarkingFunction selectTestFunction(std::string container_type,
                                       std::string algorithm_type,
                                       bool        usePrivateAllocator)
{
  // A tuple of a test-identifying parameters.
  struct TestId
  {
    std::string c; // container
    std::string a; // algorithm
    bool        p; // private
    bool operator < (TestId rhs) const
    {
      return std::tie (c, a, p) < std::tie (rhs.c, rhs.a, rhs.p);
    }
  };

  // These are all the functions that can be tested mapped by TestId.
  static std::map<TestId, BenchmarkingFunction> functionsById = 
  {
    {{"vector", "fill", false}, benchmarkFill<vector>},
    {{"vector", "fill", true}, benchmarkFill<PA_vector>},
    {{"forward_list", "fill", false}, benchmarkFill<forward_list>},
    {{"forward_list", "fill", true}, benchmarkFill<PA_forward_list>},
    {{"list", "fill", false}, benchmarkFill<list>},
    {{"list", "fill", true}, benchmarkFill<PA_list>},
    {{"multiset", "fill", false}, benchmarkFill<multiset>},
    {{"multiset", "fill", true}, benchmarkFill<PA_multiset>},
    {{"hash", "fill", false}, benchmarkFill<hash>},
    {{"hash", "fill", true}, benchmarkFill<PA_hash>},

    {{"vector", "copy", false}, benchmarkCopy<vector>},
    {{"vector", "copy", true}, benchmarkCopy<PA_vector>},
    {{"forward_list", "copy", false}, benchmarkCopy<forward_list>},
    {{"forward_list", "copy", true}, benchmarkCopy<PA_forward_list>},
    {{"list", "copy", false}, benchmarkCopy<list>},
    {{"list", "copy", true}, benchmarkCopy<PA_list>},
    {{"multiset", "copy", false}, benchmarkCopy<multiset>},
    {{"multiset", "copy", true}, benchmarkCopy<PA_multiset>},
    {{"hash", "copy", false}, benchmarkCopy<hash>},
    {{"hash", "copy", true}, benchmarkCopy<PA_hash>},

    {{"list", "insertDelete", false}, benchmarkInsertDelete<list>},
    {{"list", "insertDelete", true}, benchmarkInsertDelete<PA_list>},
    {{"multiset", "insertDelete", false}, benchmarkInsertDelete<multiset>},
    {{"multiset", "insertDelete", true}, benchmarkInsertDelete<PA_multiset>},
    {{"hash", "insertDelete", false}, benchmarkInsertDelete<hash>},
    {{"hash", "insertDelete", true}, benchmarkInsertDelete<PA_hash>},

    {{"vector", "readWrite", false}, benchmarkReadWrite<vector>},
    {{"vector", "readWrite", true}, benchmarkReadWrite<PA_vector>},
    {{"forward_list", "readWrite", false}, benchmarkReadWrite<forward_list>},
    {{"forward_list", "readWrite", true}, benchmarkReadWrite<PA_forward_list>},
    {{"list", "readWrite", false}, benchmarkReadWrite<list>},
    {{"list", "readWrite", true}, benchmarkReadWrite<PA_list>},
  };

  TestId id = { container_type, algorithm_type, usePrivateAllocator};
  if (functionsById.count(id))
    return functionsById[id];
  else
    return nullptr;
}

//========================================================================================
// Wait for pressing ENTER key.
//________________________________________________________________________________________
static void waitForKey()
{
  std::cout << "  PRESS ENTER TO FINISH:" << std::flush;
  std::cin.ignore();
  std::cout << '\n';
}

//========================================================================================
// Tries to identify a single benchmark using the supplied parameters.
// On success, runs it prints the result.
// On failure, reports error and returns false.
//________________________________________________________________________________________
static bool runSingleBenchmark(std::string container_type,
                               std::string algorithm_type,
                               std::string allocator, // "std" or "private"
                               std::string threadCount,
                               bool        waitKeypress)
{
  if (allocator != "private" && allocator != "std")
  {
    std::cout << "Error: wrong allocator arg: " << allocator << std::endl;
    return false;
  }

  // Longer duration for better accuracy while running a single benchmark
  dBenchmarkDuration = 15.0; 

  bool usePrivateAllocator = allocator == "private";
  int nThreadCount = atoi(threadCount.c_str());
  if (nThreadCount <= 0 || nThreadCount > 10)
  { 
    std::cout << "Error: invalid thread count: " << threadCount << '\n';
    return false;
  }

  auto function = selectTestFunction(container_type, algorithm_type, usePrivateAllocator);
  if (!function)
  {
    std::cout << "Error: test *NOT* identified" << std::endl;
    return false;
  }

  std::cout << "Running a single benchmark (" << container_type << "/"
            << algorithm_type << "/"<< allocator << "/"<< threadCount<< " thread)..." 
            << std::flush;
  double pa = runInParallelAndAccumulate(function, nThreadCount);
  std::cout << "completed. Rate = " << pa << "/sec." << std::endl;
  if (waitKeypress)
    waitForKey();
  return true;
}

//========================================================================================
// Declare a million of small containers.
// Intended for memory overhead assesment only.
//________________________________________________________________________________________
template <typename Container>
static void declareOneMegContainers()
{
  std::vector<Container> items(1000*1000, Container(1));
}

//========================================================================================
// Parse the command line arguments, identifies the benchmarks to run, and then runs it.
// Any errors or results are printed to the console.
//________________________________________________________________________________________
static void parseArgsRunTest(int argc, const char* argv[])
{
  static const std::string helpString = 
    "Benchmark of combinations of several C++ STL containers selectively\n"
    "paired with std::allocator<> and rg::PrivateAllocator<>.\n"
    "Measures performance (speed) and allows memory usage assessment by using\n" 
    "external (process-level) tools.\n"
    "The command-line options are:\n" 
    "  unittest[s]\n"
    "     Run built-in unittests and then exit.\n"
    "  <container> <algorithm> std|private <thread_count> [wait]\n"
    "     Benchmark particular combination of container and test:\n"
    "     <container>: vector|forward_list|list|multiset|hash\n"
    "                    (hash is for 'unordered_multiset')\n"
    "     <algorithm>:  fill|copy|insertDelete|readWrite\n"
    "     std|private: use standard or 'private' allocator, respectively\n"
    "     wait: wait for a keystroke before exit (to check memory usage).\n"
    "     NOTE: Not all combinations are valid.\n"
    "  none|all|<algorithm>\n"
    "     Run none, all, or the benchmark subset using <algorithm> only.\n"
    "  small std|private\n"
    "     Memory-usage test. Create one milion small (1-item) std::forward_list<>s.\n"
    "\n"
    "Hints for checking of memory usage at external (outside processs) level:\n"
    "  Linux:\n"
    "    $ /usr/bin/time -f \"%M k\" ./RunBenchmarks.exe <options...>\n"
    "  OSX:\n"
    "    $ /usr/bin/time -l ./RunBenchmarks.exe <options...> \n"
    "    Look at 'maximum resident set size'.\n"
    "  Windows:\n"
    "    Use 'wait' command-line option. While waiting on a key,\n"
    "    in ProcessExplorer look at the column 'Peak Private Bytes'.\n\n";

  if (argc == 2)
  {
    std::string s = argv[1];

    if (s == "none")
    {
      // ********** RUNNING NO TESTS **********
      std::cout << "Selected to run *None* benchmarks." << std::endl;
      waitForKey();
      return;
    }

    if (s == "unittests" || s == "unittest")
    { 
      // ********** RUN UNITESTS ONLY **********
      rg_unittest::runAllUnittests();
      return;
    }

    // RUN SUBSET, OR ALL BENCHMARKS:
    std::map<std::string, void(*)()> multiTests = 
    {
      { "all", rg_privateallocator::doAllSideBySideBenchmarks},
      { "fill", rg_privateallocator::doAllFillBenchmarks},
      { "copy", rg_privateallocator::doAllCopyBenchmarks},
      { "insertDelete", rg_privateallocator::doAllInsertDeleteBenchmarks},
      { "readWrite", rg_privateallocator::doAllReadWriteBenchmarks},
    };

    if (multiTests.count(s))
    { 
      std::cout << "Running multiple benchmarks: " << s << std::endl;
      multiTests[s]();
      return;
    }
  }

  if (argc == 3)
  { 
    // RUN SMALL LISTS MEMORY TESTS
    std::string p1 = argv[1], 
                p2 = argv[2];
    if (p1 != "small" || (p2 != "std" && p2 != "private"))
    {
      // Bad parameters:
      std::cout << helpString;
      return;
    }
    if (p2 == "std")
      declareOneMegContainers<forward_list>();
    else
      declareOneMegContainers<PA_forward_list>();
    waitForKey();
    return;
  }

  // ********** RUN ONE TEST **********
  if (argc < 5)
  { 
    // Not enough parameters
    std::cout << helpString;
    return;
  }

  // 'nowait':
  bool doWait = false;
  if (argc == 6)
  { 
    if (argv[5] == std::string("wait"))
      doWait = true;
    else
    { 
      std::cout << helpString;
      return;
    }
  }

  if (!runSingleBenchmark(argv[1], argv[2], argv[3], argv[4], doWait))
    std::cout << "ERROR: Failed to identify the benchmark requested\n" << helpString;
}

} // namepace rg_privateallocator


//========================================================================================
//________________________________________________________________________________________
int main(int argc, const char* argv[])
{
  rg_privateallocator::PA_list a(1), b(2);
  auto aa = a.get_allocator(), ab = b.get_allocator();
  std::swap(a, b);
  auto ba = a.get_allocator(), bb = b.get_allocator();

  rg_privateallocator::parseArgsRunTest(argc, argv);
}

// -------------------------------- End Of File ------------------------------------------
