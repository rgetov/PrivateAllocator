// BackendAllocators.h
//
// Backend alocators provide basic allocation/deallocation
// in form of static functions.
// Used to wrap back-end new/delete (or malloc/free) and optionally insert instrumentation,
// e.g. counting/high-water-mark etc.
//
// Author: 
//     Radoslav Getov, getov@mail.com  

// ------------------------------------- #Include Guards ---------------------------------

#ifndef RGCPP_BACKENDALOCATORS_H
#define RGCPP_BACKENDALOCATORS_H

// ------------------------------------- #Includes ---------------------------------------

#include <memory> // std::allocator
#include <cassert>

// ------------------------------------- Definitions -------------------------------------

namespace rg_privateallocator
{ 

//*****************************************************************************************
// Abstract base class / interface for the backend allocatoins.
// The 
//________________________________________________________________________________________
struct BackendAllocator
{
  virtual void* allocateRaw(size_t size) const = 0;
  virtual void deallocateRaw(void* b) const = 0;
};

// The BackendAllcator that PrivateAllocator<> uses for *all* (not just Page) allocations and 
// deallocations
extern const BackendAllocator* const theBackendAllocator;


//*****************************************************************************************
// BackendAllocator implementation using ::operator new() and ::operator delete()
//________________________________________________________________________________________
struct NewDeleteBackend : BackendAllocator
{
  void* allocateRaw(size_t size) const override;
  void deallocateRaw(void* b) const override;
};

// -------------------------------- End Of File ------------------------------------------

} // namespace rg_privateallocator

#endif // #include guard
