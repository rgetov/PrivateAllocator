// BackendAllocators.cpp 
//
// Implementation file
//
// Author: 
//    Radoslav Getov, getov@mail.com  

// ------------------------------------- #Includes ---------------------------------------

#include "BackendAllocators.h"
#include "Unittest.h"

// ------------------------------------- Definitions -------------------------------------

namespace rg_privateallocator
{ 

//========================================================================================
//________________________________________________________________________________________
void* NewDeleteBackend::allocateRaw(size_t size) const
{
  return ::operator new(size);
}

//========================================================================================
// Eploratory - new()/delete() statistics
//________________________________________________________________________________________
void NewDeleteBackend::deallocateRaw(void* b) const
{
  ::operator delete(b);
}

const NewDeleteBackend newDeleteBackend;

const BackendAllocator* const theBackendAllocator = &newDeleteBackend;

} // namespace rg_privateallocator


//========================================================================================
// Eploratory - new()/delete() statistics (must be in global namespace!)
//________________________________________________________________________________________
#if 0

static long long allocationCounts[1024];
void* operator new(size_t s)
{
  if (s < 1024)
    allocationCounts[s]++;
  return malloc(s);
}

void operator delete(void* p)
{
  if (p)
    free(p);
}

void printAllocStats()
{
  std::cout << "Allocations per block size:\n";
  for (int j = 0; j < 1024; ++j)
    if (auto c = allocationCounts[j])
      if (c > 10*1000*1000)
        std::cout << "  " << j << ": " << c / 1000000 << " M\n";
      else if (c > 10000)
        std::cout << "  " << j << ": " << c / 1000 << " k\n";
      else
        std::cout << "  " << j << ": " << c << "\n";
}

RG_AT_END(printAllocStats())

#endif


