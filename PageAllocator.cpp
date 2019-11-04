// PageAllocator.cpp
//
// Implementation file.
//
// Author: 
//    Radoslav Getov, getov@mail.com  

// ------------------------ #Includes ---------------------------------------

#include "PageAllocator.h"
#include "BackendAllocators.h"

#include "Unittest.h"

#include <algorithm> // min/max 
#include <cassert>  
#include <utility>

// --------------------------- Definitions -------------------------------------

namespace rg_privateallocator 
{

//////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Free functions and vars ///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

//========================================================================================
// Calculate the maximal alignment that a type of size 'size_of' may have.
//________________________________________________________________________________________
size_t calcAligmentForSize(size_t size_of)
{
  for (auto s = cnMaxAlign; /**/; s >>= 1)
    if ((size_of & (s - 1)) == 0) // same as 'size_of % s == 0'
      return s;
}

//========================================================================================
// Calculate the maximal alighment that 'ptr' is alligned to.
//________________________________________________________________________________________
size_t calcAligmentForPtr(const void* ptr)
{
  return calcAligmentForSize(size_t(ptr));
}

//========================================================================================
// Unittests for free functions
//________________________________________________________________________________________
void test_PageAllocator_Simple()
{
  RG_EXPECT(roundUp(0, 1) == 0);
  RG_EXPECT(roundUp(3, 1) == 3);
  RG_EXPECT(roundUp(3, 4) == 4);
  RG_EXPECT(roundUp(2, 8) == 8);
  RG_EXPECT(roundUp(2, 2) == 2);

  RG_EXPECT(calcAligmentForSize(12) == 4 || cnMaxAlign < 4)
  RG_EXPECT(calcAligmentForSize(13) == 1)
  RG_EXPECT(calcAligmentForSize(8) == 8 || cnMaxAlign < 8)
  RG_EXPECT(calcAligmentForSize(cnMaxAlign*10) == cnMaxAlign)
  RG_EXPECT(calcAligmentForSize(cnMaxAlign*10+1) == 1)

  RG_EXPECT(calcAligmentForPtr(nullptr) == cnMaxAlign)
  alignas(8) char str[10];
  RG_EXPECT(calcAligmentForPtr(str+1) == 1)
  RG_EXPECT(calcAligmentForPtr(str+2) == 2)
  RG_EXPECT(calcAligmentForPtr(str+5) == 1)
}

RG_ADD_UNITTEST2(test_PageAllocator_Simple, 1);


//////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// SimplePageHeader /////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

// Direct/low-level data get/set
void SimplePageHeader::setBlockSize(size_t nBlockSize)
{
  nBlockSize_ = nBlockSize;
}

void SimplePageHeader::setNextPage(Page* p)
{
  pNextPage_ = p;
}
  
Page* SimplePageHeader::getNextPage()
{
  return pNextPage_;
}

void SimplePageHeader::setFirstBlock(FreeBlock* b)
{
  pFirstBlock_ = b;
}

//========================================================================================
// SimplePageHeader unittests
//________________________________________________________________________________________
void test_SimplePageHeader()
{
  SimplePageHeader phi = {};
  phi.setBlockSize(4);
  RG_EXPECT(phi.getBlockSize() == 4);

  FreeBlock bh;
  phi.setFirstBlock(&bh);
  RG_EXPECT(phi.getFirstBlock() == &bh);

  Page ph;
  phi.setNextPage(&ph);
  RG_EXPECT(phi.getNextPage() == &ph);
}

RG_ADD_UNITTEST2(test_SimplePageHeader, 1);

//////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////// PackedPageHeader //////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

void PackedPageHeader::setBlockSize(size_t nBlockSize)
{
  nBlockSizeLSB_ = (nBlockSize >> 3) & 0x7;
  nBlockSizeMSB_ = nBlockSize >> 6;
}

void PackedPageHeader::setNextPage(Page* p)
{
  nNextPageMSB_ = size_t(p) >> 3;
}
  
Page* PackedPageHeader::getNextPage()
{
  return (Page*) (nNextPageMSB_ << 3);
}

void PackedPageHeader::setFirstBlock(FreeBlock* b)
{
  nFirstBlockMSB_ = size_t(b) >> 3;
}

//========================================================================================
// PackedPageHeader unittests
//________________________________________________________________________________________
void test_PackedPageHeader()
{
  PackedPageHeader phi = {};
  phi.setBlockSize(64);
  RG_EXPECT(phi.getBlockSize() == 64);

  FreeBlock bh;
  phi.setFirstBlock(&bh);
  RG_EXPECT(phi.getFirstBlock() == &bh);
  RG_EXPECT(phi.getBlockSize() == 64);

  Page ph;
  phi.setNextPage(&ph);
  RG_EXPECT(phi.getNextPage() == &ph);
  RG_EXPECT(phi.getFirstBlock() == &bh);
  RG_EXPECT(phi.getBlockSize() == 64);
}

RG_ADD_UNITTEST2(test_PackedPageHeader, 1);


//////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////// Page ///////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

//========================================================================================
//________________________________________________________________________________________
void Page::initialize(size_t nBlockSize, size_t nBlockCount, Page* pagesSoFar)
{
  // The layout of Page is:
  // <Page><FreeBlock...><FreeBlock...>....<FreeBlock>.

  assert (nBlockSize > 0 && nBlockCount > 0);

  // Chain the available memory as FreeBlocks
  char* pFirstBlock = (char*)this + sizeof(*this);
  char* pLastBlock = pFirstBlock + nBlockSize * (nBlockCount - 1);
  for (char* b = pFirstBlock, *next = b + nBlockSize; 
       /**/; 
       b = next, next = b + nBlockSize)
  {
    FreeBlock* bh = (FreeBlock*) b;
    if (b != pLastBlock)
    { 
      bh->pNextBlock_ = (FreeBlock*) next;
      continue;
    }
    FreeBlock* existingBlocks = pagesSoFar 
                                  ? pagesSoFar->header_.getFirstBlock() 
                                  : nullptr;
    bh->pNextBlock_ = existingBlocks;
    break;
  }

  header_.setBlockSize(nBlockSize);
  header_.setNextPage(pagesSoFar);
  header_.setFirstBlock((FreeBlock*) pFirstBlock);
}

//========================================================================================
//________________________________________________________________________________________
size_t Page::countFreeBlocks()
{
  size_t nRet = 0;
  for (auto b = header_.getFirstBlock(); b; b = b->pNextBlock_)
    ++nRet;
  return nRet;
}

//========================================================================================
// The smallest page should have at least one block but could have more due to 
// small user size and mandatory padding
//________________________________________________________________________________________
size_t Page::calcMinBlockCount(size_t nBlockSize)
{
  // Use at least as much memory as Page
  if (nBlockSize >= sizeof(Page))
    return 1;
  else
    return sizeof(Page) / nBlockSize; // Round 'down'
}

//========================================================================================
// Calculates the size (count of blocks) of the next page to be created.
//________________________________________________________________________________________
size_t Page::calcNewPageBlockCount(size_t nBlockSize, Page* pagesSoFar)
{
  size_t nFirstPageCount = calcMinBlockCount(nBlockSize);
  if (!pagesSoFar) // so, 1st page ever
    return nFirstPageCount;

  // Page sizes grow exponentially
  size_t nMaxPageBlockCount = (cnMaxPageByteSize_ - sizeof(Page)) / nBlockSize,
         nRet = nFirstPageCount;
  for (Page *page = pagesSoFar; page; page = page->header_.getNextPage())
  {
    nRet *= 2;
    if (nRet >= nMaxPageBlockCount)
      return nMaxPageBlockCount;
  }

  return nRet;
}

//========================================================================================
// Creates a new Page of the appropriate size, and chains in in front of 'pagesSoFar'.
// Returns pointer to the new Page.
//________________________________________________________________________________________
Page* Page::addNewPage(size_t nUserSize, Page* pagesSoFar)
{
  size_t nBlockSize = pagesSoFar
                        ? pagesSoFar->getBlockSize()
                        : roundUp(nUserSize, cnMinAlign);
  assert (!pagesSoFar || pagesSoFar->getBlockSize() >= nUserSize);

  size_t nBlockCount = calcNewPageBlockCount(nBlockSize, pagesSoFar);
  size_t nByteSize = nBlockCount * nBlockSize;
  void* rawMemory = theBackendAllocator->allocateRaw(sizeof(Page) + nByteSize);
  assert(calcAligmentForPtr(rawMemory) >= cnMaxAlign);

  Page* ret = (Page*) rawMemory;
  ret->initialize(nBlockSize, nBlockCount, pagesSoFar);
  return ret;
}

//========================================================================================
// Delete a linked-list of pagesSoFar.
//________________________________________________________________________________________
void Page::deleteAllPages(Page* pagesSoFar)
{
  while (pagesSoFar)
  {
    Page* next = pagesSoFar->header_.getNextPage();
    theBackendAllocator->deallocateRaw(pagesSoFar);
    pagesSoFar = next;
  }
}

//========================================================================================
// Unittests
//________________________________________________________________________________________
void test_Page()
{
  RG_EXPECT(Page::calcMinBlockCount(1) >= sizeof(Page) / cnMinAlign);
  RG_EXPECT(Page::calcMinBlockCount(sizeof(Page)) == 1)

  size_t firstPageBlocks = Page::calcMinBlockCount(cnMinAlign);
  RG_EXPECT(firstPageBlocks >= 1)
  RG_EXPECT(Page::calcNewPageBlockCount(cnMinAlign, nullptr) == firstPageBlocks)

  Page* p1 = Page::addNewPage(cnMinAlign, nullptr);
  RG_EXPECT(p1 && p1->getBlockSize() == cnMinAlign);
  size_t p2c = Page::calcNewPageBlockCount(cnMinAlign, p1);
  RG_EXPECT(p2c == 2 * firstPageBlocks) 
  Page* p2 = Page::addNewPage(cnMinAlign, p1);
  size_t nFree = p2->countFreeBlocks();
  RG_EXPECT(nFree >= firstPageBlocks + p2c);

  void* b1 = p2->takeBlock();
  RG_EXPECT(p2->countFreeBlocks() == nFree-1);
  p2->returnBlock(b1);
  RG_EXPECT(p2->countFreeBlocks() == nFree);

  Page::deleteAllPages(p2);
}

RG_ADD_UNITTEST2(test_Page, 1);


//////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// PageHandle ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

//========================================================================================
//________________________________________________________________________________________
PageHandle* PageHandle::findPrevInList()
{
  assert(pNext_ != this);

  for (auto h = pNext_; /**/; h = h->pNext_)
    if (h->pNext_ == this)
      return h;
}

//========================================================================================
//________________________________________________________________________________________
bool PageHandle::inSameList(const PageHandle* ph) const
{
  if (this == ph)
    return true;
  for (auto h = pNext_; h != this; h = h->pNext_)
    if (h == ph)
      return true;
  return false;
}

//========================================================================================
// PaHandle unittests
//________________________________________________________________________________________
void test_PaHandle()
{
  PageHandle n1, n2;
  RG_EXPECT(n1.isSingleInList() && n2.isSingleInList());

  n1.addSelfToList(&n2);
  RG_EXPECT(n1.pNext_ == &n2 && n2.pNext_ == &n1);
  RG_EXPECT(!n1.pPage_ && !n2.pPage_);

  n2.removeSelfFromList();
  RG_EXPECT(n1.isSingleInList() && n2.isSingleInList());

  n1.addSelfToList(&n2);
  auto pa = n1.getOrCreatePage(20, true);

  RG_EXPECT(pa && pa->getBlockSize() >= 20);
  RG_EXPECT(pa && pa->countFreeBlocks() >= 1);
  RG_EXPECT (n1.pPage_ == pa && n2.pPage_ == pa);

  n2.removeSelfFromList();
  RG_EXPECT(n2.isSingleInList() && !n2.pPage_ && n1.pPage_);

  n2.addSelfToList(&n1);
  RG_EXPECT(n2.pPage_ == pa);
  Page::deleteAllPages(pa);
};

RG_ADD_UNITTEST2(test_PaHandle, 1);


// ------------------------ End Of File --------------------------------------

} // namespace rg_privateallocator
