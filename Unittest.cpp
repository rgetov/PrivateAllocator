// Unittest.cpp
//
// A small ad-hoc unittest framework - implementation.
// 
// Author: 
//     Radoslav Getov, getov@mail.com  

// ------------------------------------- #Includes ---------------------------------------

#include "Unittest.h"

#include <vector>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cassert>

// ------------------------------------- Definitions -------------------------------------

namespace rg_unittest 
{

//========================================================================================
//________________________________________________________________________________________
std::string shorten(std::string from, size_t maxLen)
{
  if (maxLen < 3)
    maxLen = 3;
  if (from.size() > maxLen)
  { 
    from.erase (from.begin() + maxLen, from.end());
    from += "...";
  }

  return from;
}

// Unittest registeration and exectution

int passedChecksCounter = 0;
int failedChecksCounter = 0;

// Unittests are registered by their name and actual unittest function:
struct RegisteredTest
{
   std::string testName; 
   void (*testFunction_)();
   double orderIndex_; // before each run unittest are ordered by this value

   bool operator < (RegisteredTest rhs) const 
   { 
     return orderIndex_ < rhs.orderIndex_;
   }
};

//========================================================================================
// Contains all the registered unittests.
// Lazily allocated upon first usage, deallocated automatically.
// Being a pointer ensures non-dynamic init.
//________________________________________________________________________________________
static std::vector<RegisteredTest> *allTests = nullptr; 


// Machinery to delete 'allTests' upon porogram shutdown (after main()):
static struct AllTestDeleter
{
   ~AllTestDeleter() {delete allTests;}
} allTestsDeleter;

//========================================================================================
// Implementation - see declaration in the header for info.
//________________________________________________________________________________________
void registerTest(std::string testName, void (*testFunction)(), double orderPriority)
{
   if (!allTests)
      allTests = new std::vector<RegisteredTest>;

   allTests->push_back({testName, testFunction, orderPriority});
}

//========================================================================================
// Implementation - see declaration in the header for info.
//________________________________________________________________________________________
bool runAllUnittests()
{
   if (!allTests)
      return true;

   std::sort(allTests->begin(), allTests->end());

   passedChecksCounter = 0;

   int failedTests = 0, 
       succeededTests = 0;
   std::cerr << "Running " << allTests->size() << " registered unittests:" << std::endl;

   for (auto ru : *allTests)
   {
      using hrc = std::chrono::high_resolution_clock;
      auto start = hrc::now();

      std::cerr << "  " << ru.testName << ": " << std::flush;

      int oldFailed = failedChecksCounter,
          oldOk = passedChecksCounter;
      try
      { 
        ru.testFunction_();
        bool ok = failedChecksCounter == oldFailed;
        if (ok)
         {
            std::cerr << "PASS";
            ++succeededTests;
         }
         else
         { 

            std::cerr << "FAIL";
            ++failedTests;
         }
      }
      catch (...)
      {
         std::cerr << "FATAL FAILURE - UNCAUGHT EXCEPTION";
      }
      
      int checksThisSet = (failedChecksCounter + passedChecksCounter) 
                          - (oldFailed + oldOk);

      std::chrono::duration<double> elapsed = hrc::now() - start;
      double time = std::max(elapsed.count() - 0.01, 0.0);
      std::cout << " (" << checksThisSet << " checks; " << time << " sec)" << '\n';
   }

   std::cerr << passedChecksCounter << " checks passed, " 
     << failedChecksCounter << " failed." 
     << '\n' << succeededTests << " unittest passed, " << failedTests << " failed." 
     << '\n' << "---------------------------------------------------" << std::endl;

   return failedTests == 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Unittests self-unittest /////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

// Test RG_AT_START:
static int RG_AT_START_tester = 3;
RG_AT_START(assert(RG_AT_START_tester==3); RG_AT_START_tester = 5;) // should exist
RG_AT_START(assert(RG_AT_START_tester == 5);) // should have been modified

#if 0 // Enable if you are testing the unittest framework itself
// Deliberately-failing unittests:
RG_ADD_UNITTEST("deliberately-failing-expect", 
                [](){RG_EXPECT(false)}, 
                0.1);

RG_ADD_UNITTEST("deliberately-failing-throw", 
                [](){RG_MUST_THROW(int x=0)}, 
                0.2);

RG_ADD_UNITTEST("deliberately-failing-long", 
                [](){RG_MUST_THROW(auto x = "this is very long should be truncated")},
                0.2);

// Alwasy-successful unittest:
RG_ADD_UNITTEST("deliberately-successful", 
                []()
                {
                  RG_EXPECT(true); 
                  RG_MUST_THROW(throw 1);
                }, 
                1);
#endif

// -------------------------------- End Of File ------------------------------------------

} // namespace rg_unittest

