// PageAllocator.h
// 
// Fixed-size block allocator allocating large pages exponentially-increasing size
//
// Author: 
//    Radoslav Getov, getov@mail.com  

// ------------------------------------- #Include Guards ---------------------------------

#ifndef RG_PAGEALLOCATOR_H_INCLUDED
#define RG_PAGEALLOCATOR_H_INCLUDED

// ------------------------------------- #Includes ---------------------------------------

#include <utility>  // std::swap
#include <cstdio> 
#include <cstddef> // max_align_t
#include <cassert>
#include <algorithm>

// ------------------------------------- Definitions -------------------------------------

namespace rg_privateallocator
{


///////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// Constants and free functions //////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


// The maximal size that PrivateAllocator considers for management.
const size_t cnMaxBlockSize_ = 128;

// The maximal size (in bytes) that of a single Page 
const size_t cnMaxPageByteSize_ = 100*1000u; 

// User sizes and all adresses are assumed/forced to round/align to this:
const size_t cnMinAlign = sizeof(void*) > 8 ? sizeof(void*) : 8;

// Just an alias of std-imposed value
const size_t cnMaxAlign = alignof(::std::max_align_t);

static_assert (cnMaxAlign >= cnMinAlign, "cnMaxAlign < cnMinAlign");


//========================================================================================
// Smallest value >= 'value' divisible by 'alighment' (alignment assumed to be power of 2)
//________________________________________________________________________________________
size_t roundUp(size_t value, size_t alignment);

//========================================================================================
// Calculate the maximal alighment that a type of size 'size_of' may have.
//________________________________________________________________________________________
size_t calcAligmentForSize(size_t size_of);

//========================================================================================
// Calculate the maximal alighment that 'ptr' is alligned to.
//________________________________________________________________________________________
size_t calcAligmentForPtr(const void* ptr);

//========================================================================================
//________________________________________________________________________________________
inline size_t roundUp(size_t value, size_t alignment)
{
  assert (alignment > 0); 
  assert ((alignment & (alignment - 1)) == 0); // should be power of 2

  return (value + alignment - 1) & ~(alignment - 1); 
}

//****************************************************************************************
// (Free-block *header* in Page
// Actual blocks are of potentially larger size.
// Blocks are alligned on at least cnMinAlign; higher alignment may come due
// to requested user size.
//________________________________________________________________________________________
struct alignas(cnMinAlign) FreeBlock 
{
  // User data start here while blocks are *used*.
  FreeBlock* pNextBlock_; // Linked list of *free* blocks; 
};

class Page; // fwd

//////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// SimplePageHeader /////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


//****************************************************************************************
// Simple (as opposite to 'packed') Page header data
//________________________________________________________________________________________
class alignas(cnMaxAlign) SimplePageHeader
{
  size_t        nBlockSize_; 
  Page*         pNextPage_;
  FreeBlock* pFirstBlock_; 

public:
  
// Direct/low-level data get/set
  void setBlockSize(size_t);
  size_t getBlockSize();

  void setNextPage(Page* p);
  Page* getNextPage();

  void setFirstBlock(FreeBlock* b);
  FreeBlock* getFirstBlock();
};

inline size_t SimplePageHeader::getBlockSize()
{
  return nBlockSize_;
}

inline FreeBlock* SimplePageHeader::getFirstBlock()
{
  return pFirstBlock_;
}

//////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// PackedPageHeader //////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


//****************************************************************************************
// Alternative to SimplePageHeader.
// Here data is packed in bitfields in order to fit to 2*sizeof(size_t)
//________________________________________________________________________________________
class alignas(cnMaxAlign) PackedPageHeader
{
  static const size_t sizeBits = 8 * sizeof(size_t) - 3;
  size_t nNextPageMSB_  : sizeBits; // Next page ptr >> 3
  size_t nBlockSizeMSB_ : 3;        // half the bits of block size
  size_t nFirstBlockMSB_: sizeBits; // First block ptr >> 3
  size_t nBlockSizeLSB_ : 3;        // the other half of the bits for block size
public:
// Direct/low-level data get/set
  void setBlockSize(size_t nBlockSize);
  size_t getBlockSize();

  void setNextPage(Page* p);
  Page* getNextPage();

  void setFirstBlock(FreeBlock* b);
  FreeBlock* getFirstBlock();
};

static_assert(sizeof(PackedPageHeader) == 2 * sizeof(size_t), 
              "PackedPageHeader too large");

inline size_t PackedPageHeader::getBlockSize()
{
  return (nBlockSizeMSB_ << 6) + (nBlockSizeLSB_ << 3);
}

inline FreeBlock* PackedPageHeader::getFirstBlock()
{
  return (FreeBlock*) (nFirstBlockMSB_ << 3);
}

//////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// Page /////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

//****************************************************************************************
// A single Page comprizing the pool of free blocks that PrivateAllocator<> uses.
// 
// The actual page memory layout is:
// <Page> <FreeBlock...> <FreeBlock...> ... <FreeBlock...> 
//   where FreeBlocks are of size PageHeader::getBlockSize()
// Pages are (raw)-allocated on demand and linked in a list served for disposal only.
// Upon creation of each page its blocks are initially chained and added to the free-block 
// list in Pag::.header_. Thereafter, all allocation and deallocation requests are
// served from/to this list.
//________________________________________________________________________________________
class alignas(cnMaxAlign) Page
{
  // Alternate between SimplePageHeader and PackedPageHeader:
  #if 0
    typedef SimplePageHeader PageHeader;
  #else
    typedef PackedPageHeader PageHeader;
  #endif

  PageHeader header_;

public:
  size_t getBlockSize();

  bool hasFreeBlocks(); 
  void* takeBlock(); // always succeeds
  void returnBlock(void* block);

  void initialize(size_t nBlockSize, size_t nBlockCount, Page* pagesSoFar);

  static size_t calcMinBlockCount(size_t nBlockSize); // For the 1st page
  static size_t calcNewPageBlockCount(size_t nBlockSize, Page* pagesSoFar);
  static Page* addNewPage(size_t nUserSize, Page* pagesSoFar);
  static void deleteAllPages(Page* pFirstPage);
  size_t countFreeBlocks();
};

inline size_t Page::getBlockSize()
{
  return header_.getBlockSize();
}

inline bool Page::hasFreeBlocks()
{
  return header_.getFirstBlock() != nullptr;
}

inline void* Page::takeBlock()
{
  auto fb = header_.getFirstBlock();
  assert(fb); // should have been checked outside
  header_.setFirstBlock(fb->pNextBlock_);
  return fb;
}

inline void Page::returnBlock(void* block)
{
  assert(block);
  auto bh = (FreeBlock*) block;
  bh->pNextBlock_ = header_.getFirstBlock();
  header_.setFirstBlock(bh);
}

//////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// PageHandle ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


//****************************************************************************************
// A node participating in circular list of items sharing the same Page (through pointer).
// Embedded as data member in PrivateAllocator<>.
//________________________________________________________________________________________
struct PageHandle 
{
// Data
  Page* pPage_ = nullptr; // Delay-created/updated
  PageHandle* pNext_;  // Form a circular list of the co-owners of a Page

// Ctors
  PageHandle() { pNext_ = this; }
  
  PageHandle(const PageHandle&) = delete;
  PageHandle& operator=(const PageHandle&) = delete;

// (Circular) list management
  bool isSingleInList() const;
  PageHandle* findPrevInList();
  void addSelfToList(PageHandle* where);
  void removeSelfFromList();
  bool inSameList(const PageHandle* ph) const;

// Page access and creation 
  Page* getOrCreatePage(size_t nUserBlockSize, bool needFreeBlock);
};

inline bool PageHandle::isSingleInList() const
{
  return pNext_ == this;
}

inline void PageHandle::addSelfToList(PageHandle* where)
{
  assert (where && this != where);

  pPage_ = where->pPage_;
  pNext_ = where->pNext_;
  where->pNext_ = this;
}

inline void PageHandle::removeSelfFromList()
{
  assert (!isSingleInList());

  PageHandle* prev = findPrevInList();
  prev->pNext_ = pNext_;
  pNext_ = this;
  pPage_ = nullptr;
}

inline Page* PageHandle::getOrCreatePage(size_t nUserBlockSize, bool needFreeBlock)
{
  if (!needFreeBlock)
    return pPage_;

  if (!pPage_ || !pPage_->hasFreeBlocks())
  {
    pPage_ = Page::addNewPage(nUserBlockSize, pPage_);
    // Set all buddies' .pPage_ to the newly-created one:
    for (auto h = pNext_; h != this; h = h->pNext_)
      h->pPage_ = pPage_;
  }
  assert (pPage_ && pPage_->getBlockSize() >= nUserBlockSize);
  return pPage_;
}

// -------------------------------- End Of File ------------------------------------------

} // namespace rg_privateallocator

#endif  // #include guard


