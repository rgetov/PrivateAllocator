// A small ad-hoc unittest framework. 
//
// Defines these:
//    #RG_EXPECT(code)
//    #RG_MUST_THROW(code)
//       Each creates a unittest. They are to be used inside a test set function.
//
//    #RG_ADD_UNITTEST
//       registers a unittest set function (with signature: 'void (void)')
//    bool runAllUnittests();
//       execute all registered test sets and unintest therein.
//
// The flow is as follows:
//    - a *unittest* is a function with a signature 'void(void)'
//    - Inside each one, tzero or more *checks* are added by using 
//      #RG_EXPECT and #RG_MUST_THROW
//    - The unittest function is *registered* by invoking #RG_ADD_UNITTEST 
//      *at global scope* by suppling a *test name*, the unitest function, 
//      and an order index.
//      Alternatively, #RG_ADD_UNITTEST2 uses the name of the function
//      as a unittest name.
//    - multiple unittest may be added in each compilation unit (.cpp file)
//    - An explicit call of 'runAllUnittest()' (inside main()) executes all the tests.
// The test runner reports the name each unittest, its result (pass or fail), 
// and the time that it took to run.
//
// The test registerations happens at program initialization time (before main() 
// is called). Therefore, it is paramount that the unittests are run inside 'main()' 
// in order to ensure that all the unittest functions have been properly registerated.
//
// All items (except for macros) reside in namespace 'rg_unittest'.
//
// Author: 
//     Radoslav Getov, getov@mail.com  

// ------------------------------------- #Include Guards ---------------------------------

#ifndef RG_UNITTEST_H_INCLUDED
#define RG_UNITTEST_H_INCLUDED

// ------------------------------------- #Includes ---------------------------------------

#include <string> // string
#include <iostream> // cerr

// ------------------------------------- Definitions -------------------------------------

//========================================================================================
// --- Helper macros NOT INTENDED BY DIRECT USAGE BY THE END USER ---
//________________________________________________________________________________________

// RG_LINE: current __LINE__ as string. For info see e.g.:
// https://stackoverflow.com/questions/19343205/c-concatenating-file-and-line-macros
#define RG_S1(x) #x
#define RG_S2(x) RG_S1(x)
#define RG_LINE "Line " RG_S2(__LINE__)

// Macro helpers for creating unique semi-anonymous C++ names:
// Expample: two distinct unique anonymous names in the same block:
//   int RG_NAME_BY_LINE(a),  // Something like a286 
//       RG_NAME_BY_LINE(a);  // Valid because unique, e.g. a287
#define RG_NAME_BY_LINE1(prefix,x)   prefix##x                    // helper
#define RG_NAME_BY_LINE2(prefix,x)   RG_NAME_BY_LINE1 (prefix,x)  // another helper
#define RG_NAME_BY_LINE(prefix)      RG_NAME_BY_LINE2  (prefix, __LINE__) // with prefix

// Execute 'code' at globals' dynamic initialization time (i.e. *before* main()).
// Example:
//     RG_AT_START (cout << "executed before main()!\n")
#define RG_AT_START(code)                          \
                                                   \
   namespace /*also forces external scope*/        \
   {                                               \
      struct RG_NAME_BY_LINE(AtStarter_)           \
      {                                            \
         RG_NAME_BY_LINE(AtStarter_)()   /*ctor*/  \
         {                                         \
            code;                                  \
         }                                         \
      }                                            \
                                                   \
      RG_NAME_BY_LINE (atStartVar_);               \
   }  /*anonymous namespace*/

// Execute 'code' at globals' dynamic *destruction* time (i.e. *after* main() return).
// Example:
//     RG_AT_END (cout << "executed after main()!\n")
#define RG_AT_END(code)                            \
                                                   \
   namespace /*also forces external scope*/        \
   {                                               \
      struct RG_NAME_BY_LINE(AtEnder_)             \
      {                                            \
         ~RG_NAME_BY_LINE(AtEnder_)()   /*dtor*/   \
         {                                         \
            code;                                  \
         }                                         \
      }                                            \
                                                   \
      RG_NAME_BY_LINE (atEndVar_);                 \
   }  /*anonymous namespace*/

// --- END Helper macros --- 

namespace rg_unittest
{

// The counter for all the unittests *checks* been successfully run.
extern int passedChecksCounter;
extern int failedChecksCounter;


//========================================================================================
// Invoke 'code' as a single unittest.
// Verifies that 'code' evaluated to true. 
// Updates global counters.
// Issues a report on failure.
//________________________________________________________________________________________
#define RG_EXPECT(code)                 \
{                                       \
  if (!(code))                          \
  {                                     \
    std::cerr << "(" << RG_LINE << ": Failed: '"         \
              << ::rg_unittest::shorten(#code) << "') "; \
    ++rg_unittest::failedChecksCounter;    \
  }                                        \
  else                                     \
    ++::rg_unittest::passedChecksCounter;  \
}

//========================================================================================
// Invoke 'code' as a single unittest.
// Verifies that 'code' threw an exception.
// Updates global counters.
// Issues a report on failure.
//________________________________________________________________________________________
#define RG_MUST_THROW(code)             \
{                                       \
  bool thrown = false;                  \
  try { code; }                         \
  catch(...) { thrown = true; }         \
  if (!thrown)                          \
  {                                     \
    std::cerr << "(" << RG_LINE << ": Didn't throw: '"   \
              << ::rg_unittest::shorten(#code) << "') "; \
    ++::rg_unittest::failedChecksCounter;  \
  }                                        \
  else                                     \
    ++::rg_unittest::passedChecksCounter;  \
}

//========================================================================================
// Register a user-provided function 'testFunction' in the global unittest collection.
// 'testName' is what the test runner will report.
// 'orderPriority' is used to order the unittests before running them.
//________________________________________________________________________________________
void registerTest(std::string testName, void (*testFunction)(), double orderPriority);

//========================================================================================
// Register a user funtion as unittest set. Invokes registerTest()
// at the time of global's dynamic initialization (i.e. before 'main()' is called).
// To be invoked in the file scope of the user's code.
// - testName is the name that the test runner will report
// - testFunciton is a function containing the unittest (0 or more *checks*)
// - orderPriority determines the order in which the registered unittests would be run
//________________________________________________________________________________________
#define RG_ADD_UNITTEST(testName, testFunction, orderPriority) \
                                                               \
   RG_AT_START(::rg_unittest::registerTest(testName, testFunction, orderPriority))

// Function name as test name
#define RG_ADD_UNITTEST2(testFunction, orderPriority) \
                                                      \
   RG_ADD_UNITTEST(#testFunction "(), " RG_LINE, testFunction, orderPriority)
   
//========================================================================================
// User-accessible function.
// Runs all the registered unittests. To be called within main().
//________________________________________________________________________________________
bool runAllUnittests();

// Shorten a string to a max length (for internal usage)
std::string shorten(std::string from, size_t maxLen = 30u);

// -------------------------------- End Of File ------------------------------------------

} // namespace rg_unittest

#endif // #include guard

