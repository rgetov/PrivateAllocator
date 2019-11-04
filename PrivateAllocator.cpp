// PrivateAllocator.cpp 
// 
// Unittests only (PrivateAllocator<> is template and doesn't need implementation)
// 
// Author: 
//     Radoslav Getov, getov@mail.com  


// ------------------------ #Includes ---------------------------------------

#include "PrivateAllocator.h"

#include "Unittest.h"

#include <vector>
#include <list>
#include <deque>
#include <forward_list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <string>

// --------------------------- Definitions -------------------------------------

namespace rg_privateallocator
{ 
//========================================================================================
// Unittest for PrivateAllocator<>
//________________________________________________________________________________________
void test_PrivateAllocator()
{
  typedef PrivateAllocator<int> PAI;
  typedef PrivateAllocator<long> PAIL;

  PAI pai;
  RG_EXPECT(!pai.paHandle_.pPage_);
  
  auto block3 = pai.allocate(3); // Should invoke back-end allocator
  RG_EXPECT(!pai.paHandle_.pPage_);
  for (int j = 0; j < 3; ++j)
    block3[j] = j;
  RG_EXPECT(std::vector<int>({0,1,2}) == std::vector<int>(block3, block3 + 3))
  pai.deallocate(block3, 3);

  auto block = pai.allocate(1);
  RG_EXPECT(block);
  auto pa = pai.paHandle_.pPage_;
  RG_EXPECT(pa);

  *block = 17;
  pai.deallocate(block, 1);

  block = pai.allocate(1);

  PAI cpy (pai);
  RG_EXPECT(pai == cpy);
  PAI other;
  RG_EXPECT(pai != other)

  PAIL pail(pai);
  RG_EXPECT(pail == pai)
  auto bl = pail.allocate(1);
  RG_EXPECT(bl)
};

RG_ADD_UNITTEST2(test_PrivateAllocator, 2)

} // namespace


// Compilability and unittests for most of the standard AllocatorAwareContainers
// (see https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer)
// paired with PrivateAllocator<>

using namespace rg_privateallocator;

#define STD_CONTAINER(container)   \
  typedef std::container<int, PrivateAllocator<int>> PA_##container; \
  template class std::container<int, PrivateAllocator<int>>;

STD_CONTAINER(deque)
STD_CONTAINER(forward_list)
STD_CONTAINER(list)
STD_CONTAINER(vector)

// Typedefs and explicit instatiations:
typedef std::set<int, std::less<int>, PrivateAllocator<int>> PA_set; 
template class std::set<int, std::less<int>, PrivateAllocator<int>>;

typedef std::multiset<int, std::less<int>, PrivateAllocator<int>> PA_multiset; 
template class std::multiset<int, std::less<int>, PrivateAllocator<int>>;

typedef std::unordered_set<int, 
                           std::hash<int>, 
                           std::equal_to<int>, 
                           PrivateAllocator<int>> PA_unordered_set; 
template class std::unordered_set<int, 
                                  std::hash<int>, 
                                  std::equal_to<int>, 
                                  PrivateAllocator<int>>;

typedef std::unordered_multiset<int, 
                                std::hash<int>, 
                                std::equal_to<int>, 
                                PrivateAllocator<int>> PA_unordered_multiset; 
template class std::unordered_multiset<int, 
                                       std::hash<int>, 
                                       std::equal_to<int>, 
                                       PrivateAllocator<int>>;

template <typename C>
void testPA_Container()
{
  // Default ctor 
  {
    C a;
    RG_EXPECT (a.empty());
  }
  // Copy ctor
  {
    C a {1};
    RG_EXPECT(!a.empty())
    C b (a); 
    RG_EXPECT(a == b)
  }
  // Move ctor
  {
    C a{1};
    C b (std::move(a)); 
    RG_EXPECT(!b.empty() && a.empty());
  }
  // Copy assignment
  {
    C a {1}, b;
    b = a; 
    RG_EXPECT(a == b)
    RG_EXPECT(b.get_allocator() != a.get_allocator())
  }
  // Move assignment
  {
    C a {1}, b;
    b = std::move(a); 
    RG_EXPECT(!b.empty() && a.empty());
  }
  // Swap
  {
    C a{1}, b;
    auto aa = a.get_allocator(), 
         ab = b.get_allocator();
    RG_EXPECT(aa != ab);

    std::swap(a, b);
    RG_EXPECT(a.empty() && !b.empty());

    auto ba = a.get_allocator(), 
         bb = b.get_allocator();
    RG_EXPECT(ba == ab && bb == aa); // allocators should have been swapped
  }
}

#define RG_TEST(type) \
  RG_ADD_UNITTEST("PrivateAllocator: "#type, testPA_Container<type>, 2)

RG_TEST(PA_forward_list)
RG_TEST(PA_vector)
RG_TEST(PA_list)
RG_TEST(PA_deque)
RG_TEST(PA_set)
RG_TEST(PA_multiset)
RG_TEST(PA_unordered_set)
RG_TEST(PA_unordered_multiset)

// ------------------------ End Of File --------------------------------------
