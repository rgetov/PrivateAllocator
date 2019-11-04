// PrivateAllocator.h
//
// Private (per container instance) C++ STL Allocator
// Relevant info: 
// https://en.cppreference.com/w/cpp/named_req/Allocator
// https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer
// https://foonathan.net/2015/10/allocatorawarecontainer-propagation-pitfalls/
//
// Author: 
//    Radoslav Getov, getov@mail.com  

// ------------------------------------- #Include Guards ---------------------------------

#ifndef RGCPP_PRIVATEALLOCATOR_H_INCLUDED
#define RGCPP_PRIVATEALLOCATOR_H_INCLUDED

// ------------------------------------- #Includes ---------------------------------------

#include "PageAllocator.h"
#include "BackendAllocators.h"

#include <memory> 
#include <cassert>

// ------------------------------------- Definitions -------------------------------------

namespace rg_privateallocator
{ 

//****************************************************************************************
// Private (per container instance) C++ STL Allocator
//________________________________________________________________________________________
template <typename T> 
class PrivateAllocator 
{
public:
// See https://en.cppreference.com/w/cpp/named_req/Allocator
  using value_type = T;

// Ctors, dtor:
  PrivateAllocator();
  PrivateAllocator(const PrivateAllocator& from) noexcept; 
  // Copy ctor is OK when 'move' is required

  template <typename Other> 
  explicit PrivateAllocator(const PrivateAllocator<Other>& other) noexcept; 

  ~PrivateAllocator();

// Assignments:
  // Move: used in container 'swap()'. Invokes *copy* ctor.
  void operator = (PrivateAllocator&& rhs) noexcept; 

  // Copy: used in container assigments. Invokes *default* ctor.
  void operator = (const PrivateAllocator& rhs) noexcept; 

// Allocation/deallocation:
 	T* allocate(size_t n);
 	void deallocate(T* p, size_t n) noexcept;

// Implement 'Allocator concept' flags; see:
// https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer
  // All allocators are *not* equal:
	using is_always_equal = std::false_type;
  // Containers use this function in copy ctors:
  PrivateAllocator select_on_container_copy_construction() const;
  // Containers *will* replace the destination allocator on move assignment:
  using propagate_on_container_move_assignment = std::true_type;
  // Containers *will* replace the destination allocator on copy assignment:
  using propagate_on_container_copy_assignment = std::true_type;
  // Swap the allocators in container 'swap':
	using propagate_on_container_swap = std::true_type;

// Implementation
private:
  static const size_t cnBlockSize_ = sizeof(T);
  // Whether to use the Page allocation for allocate()/deallocate() of 'n' items
  bool shouldUsePageAllocation(size_t n); 

public: // Used in global operator==()
  PageHandle paHandle_; 
};

//========================================================================================
// Equality operators as per https://en.cppreference.com/w/cpp/named_req/Allocator
//________________________________________________________________________________________
template <class T, class U>
bool operator == (PrivateAllocator<T> const& lhs, PrivateAllocator<U> const& rhs) noexcept
{
  auto p1 = lhs.paHandle_.pPage_,
       p2 = rhs.paHandle_.pPage_;
  if (p1 || p2)
    return p1 == p2;
  bool bRet = lhs.paHandle_.inSameList(&rhs.paHandle_);
  return bRet;
}

template <class T, class U>
bool operator != (PrivateAllocator<T> const& lhs, PrivateAllocator<U> const& rhs) noexcept
{
    return !(lhs == rhs);
}

//========================================================================================
//________________________________________________________________________________________
template <typename T>
PrivateAllocator<T>::PrivateAllocator()
{
}
  
//========================================================================================
// Insert itself into 'from's clique-list
//________________________________________________________________________________________
template <typename T>
PrivateAllocator<T>::PrivateAllocator(const PrivateAllocator& from) noexcept
{
  PageHandle* fromPaHandle = const_cast<PageHandle*>(&from.paHandle_);
  paHandle_.addSelfToList(fromPaHandle);
}

//========================================================================================
// Insert itself into 'from's clique-list
//________________________________________________________________________________________
template <typename T>
template <typename Other>
PrivateAllocator<T>::PrivateAllocator(const PrivateAllocator<Other>& from) noexcept
{
  // Same as copy ctor
  PageHandle* fromPaHandle = const_cast<PageHandle*>(&from.paHandle_);
  paHandle_.addSelfToList(fromPaHandle);
}

//========================================================================================
//________________________________________________________________________________________
template <typename T>
PrivateAllocator<T>::~PrivateAllocator()
{
  if (paHandle_.isSingleInList())
  { 
    if (paHandle_.pPage_)
      Page::deleteAllPages(paHandle_.pPage_);
  }
  else
    paHandle_.removeSelfFromList();
}

//========================================================================================
// Move assighment is used in container *swap*.
//________________________________________________________________________________________
template <typename T>
void PrivateAllocator<T>::operator = (PrivateAllocator&& rhs) noexcept
{
  this->~PrivateAllocator();
  new (this) PrivateAllocator(rhs); // Invoke copy ctor, participate in clique ownership.
}

//========================================================================================
// Copy assignment is used in container *assignments*.
//________________________________________________________________________________________
template <typename T>
void PrivateAllocator<T>::operator = (const PrivateAllocator& rhs) noexcept
{
  if (this != &rhs)
  { 
    this->~PrivateAllocator();
    new (this) PrivateAllocator(); // Default ctor
  }
}

//========================================================================================
// The only requests serverd by the Page allocator are for:
//   - signle blocks 
//   - that are small enough 
//________________________________________________________________________________________
template <typename T>
inline bool PrivateAllocator<T>::shouldUsePageAllocation(size_t n) 
{
  if (n != 1 || cnBlockSize_ > cnMaxBlockSize_)
    return false;

  Page* pa = paHandle_.pPage_;
  bool ret = !pa || cnBlockSize_ <= pa->getBlockSize();;
  return ret;
}

//========================================================================================
//________________________________________________________________________________________
template <typename T>
T* PrivateAllocator<T>::allocate(size_t n)
{
  void* ret;
  if (shouldUsePageAllocation(n))
  { 
    Page* page = paHandle_.getOrCreatePage(cnBlockSize_, true /*needFreeBlock*/);
    assert(page);
    ret = page->takeBlock();
  }
  else
    ret = theBackendAllocator->allocateRaw(n * cnBlockSize_);
  return static_cast<T*>(ret);
}

//========================================================================================
//________________________________________________________________________________________
template <typename T>
void PrivateAllocator<T>::deallocate(T* p, size_t n) noexcept
{
  if (shouldUsePageAllocation(n))
  { 
    Page* page = paHandle_.getOrCreatePage(cnBlockSize_, false /*needFreeBlock*/);
    assert (page); // Should have been there during allocate()
    page->returnBlock(p);
  }
  else
    theBackendAllocator->deallocateRaw(p);
}

//========================================================================================
//________________________________________________________________________________________
template <typename T>
PrivateAllocator<T> PrivateAllocator<T>::select_on_container_copy_construction() const
{
  return PrivateAllocator();
}

// -------------------------------- End Of File ------------------------------------------

} // namespace rg_privateallocator

#endif // #include guard
