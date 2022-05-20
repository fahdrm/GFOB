#include <iostream>
#include <climits>
#include <bitset>
//#include <sched.h>


/*
*******************************************************
*******************************************************
*******************************************************
*******************************************************
README.TXT
*******************************************************
*******************************************************
*******************************************************
*******************************************************

Requirements:
c++11 min
gcc 9.4.1

some decisions and Assumptions:
keeping most UDTs with public default access to minimize code, prod quality should
use proper memeber access controls and fields

I have decided to using contigous memory where every possible to make the
program cache freindly, some of the data structures I created could have
been linked lists or other dynamically created stores
for example I needed a min heap and max heap, I could have created a linked list
based heap that would grow and shrink as we go, but that dta structure is not
cache friedly so intead I used a bitset map. it turned out to be very efficient

even the OB type itself, I wanted to create it on stack, but I kept getting
crash dumped for large sizes, cuz I'm running on a WSL ubunto
if you have a large memory, which you should for OrderBook process
you can pin the process to a single core, disable hyper threading and create
most Data structures on stack.


**not testing for int < 0 in main loop, it will just add an extra check
** ArrayPresenceMap only works with positive integers
** didnt write any destructors, they are all trivial to code
** just assuming will reclaim all mem at end of main.

assuming
** T + N could go beyond MAX_NO_OF_ENTRIES
** but individualy T and N will remain below MAX_NO_OF_ENTRIES
** to code the above scenario is trivial, I'm just trying to save a branch
** and personally, I prefer having boundry conditions tested out of hot path
** hot path should not check for input validity, just makes it slow
** once you enter the hot path.. you should be all set, ready to fire all instructions



How To compile and run:
using simple cmake: nothing special, thats why I kept all code in one file
compile with cmake, or simply do

g++ -o programName main.cpp

cmake's target name is OB, to start just run OB executable


if you want to change the size of OB, edit this variable declared just before main
constexpr int maxOrderId = 128000000;

when starting, give the program some time to warm up, it takes about 5 seconds to get
memory for 128000000.
I'm testing on windows WSL ubunto, so I did not try to pin the process on any CPU
I did add code for pinning, did not usit it though.
if you want to try it out, include sched.h at the top and uncomment top lines from
main method


*/



//111101000010010000000000001
//111101000010010000000000000

//TODO: change this into a template
class ArrayPresenceMap
{
public:
  std::bitset<64> *presence;

  ArrayPresenceMap *parent;
  //aligning to keep small size
  int size;
  int numOfControllers;

  ArrayPresenceMap(int _size): size(_size), parent(nullptr)
  {
    if(size <= 64)
    {
      presence = new std::bitset<64>();
    }
    else if(size > 64)
    {
      numOfControllers = size/64;
      if(size % 64 != 0)
      {
        ++numOfControllers;
      }
      presence = new std::bitset<64>[numOfControllers];
      parent = new ArrayPresenceMap(numOfControllers);
    }
  }

  void set(int bit, bool val = true)
  {

    if(parent != nullptr)//TODO: remove the branch
    {
      int presenceNumber = bit/64;
      int offset = bit % 64;
      presence[presenceNumber].set(offset, val);
      if(val == true)
      {
        parent->set(presenceNumber, val);
      }
      else
      {
        if(presence[presenceNumber].none())
        {
          parent->set(presenceNumber, val);
        }
      }
    }
    else
    {

      presence[0].set(bit, val);
    }
  }

  bool any()
  {
    if(parent == nullptr)
    {
      return presence[0].any();
    }

    return parent->any();
  }

  //TODO: remove these 64 lits
  int firstPresent()
  {
    if(parent == nullptr)
    {
      for(int i = 0; i < 64; ++i)
      {
        if(presence[0].test(i))
        {
          return i;
        }
      }
      return -1;//TODO: are we going to get here.?
    }

    int parentNumber = parent->firstPresent();

    if(parentNumber == -1)
    {
      return -1;
    }

    for(int i = 0; i < 64; i++)
    {
      if(presence[parentNumber].test(i))
      {
        return parentNumber*(64) + i;
      }
    }
    return 0;//control wont get here::TODO test
  }

  int maxPresent()
  {
    int i = 64;
    if(parent == nullptr)
    {

      for(;i--;)//loop down is faster
      {
        if(presence[0].test(i))
        {
          return i;
        }
      }
      return -1;//TODO: are we going to get here.?
    }

    int parentNumber = parent->maxPresent();

    if(parentNumber == -1)
    {
      return -1;
    }

    i = 64;
    for(;i--;)
    {
      if(presence[parentNumber].test(i))
      {
        return parentNumber*(64) + i;
      }
    }
    return 0;//control wont get here::TODO test

  }
};

class alignas(64) Order//make it cache aware/freindly by forcing alignment at right boundry
{
public:
  int32_t id = 0;
  int32_t N = 0;
  Order* expiringNext = nullptr;

  Order(){}
};

class alignas(64) OrderBookEntry
{
public:

  Order* expiringHead = nullptr;
  Order* expiringTail = nullptr;

  OrderBookEntry(){}

  void addExpiringOrder(Order* order)
  {

    if(expiringHead == nullptr)
    {
      expiringHead = order;
    }
    else
    {
      expiringTail->expiringNext = order;
    }
    expiringTail = order;
  }


};

struct ResultAtT
{
  int nextToExpire = 0;
  int maxN = 0;
  int noOfLiveOrders = 0;
  int noOfOrdersExpiringAtT = 0;

  ResultAtT() = default;
  ~ResultAtT() = default;
};

template<int NO_OF_ENTRIES>
class OrderBook
{
public:
  Order orders[NO_OF_ENTRIES];// create an order store, prefer stack
  OrderBookEntry entries[NO_OF_ENTRIES];
  int32_t maxNArray[NO_OF_ENTRIES] = {0};//creating this separately cuz adding it entry class will use extra space due to padding
  int32_t currentT = 0;//not using unsigned cuz they need extra care from compiler and processor, eats up CPU time
  int32_t maxN = INT_MIN;
  int32_t expiringNext = INT_MAX;
  int32_t killed = 0;
  ArrayPresenceMap *expiringMap=nullptr;
  ArrayPresenceMap *maxNMap=nullptr;
  OrderBookEntry outBoundEntry;
  int32_t expiringBeyondTime = INT_MAX;

  OrderBook(){
    expiringMap = new ArrayPresenceMap(NO_OF_ENTRIES);
    maxNMap = new ArrayPresenceMap(NO_OF_ENTRIES);
  }
  ~OrderBook()
  {
    delete expiringMap;
    delete maxNMap;
  }

  void addOrder(int32_t N, ResultAtT* result)//NO return to force RVO
  {
    maxN = std::max(maxN, N);
    expiringNext = std::min(expiringNext, currentT + N);

    int expiringAtT = 0;
    //we know currentT is always valid, so doing no checks for validity
    orders[currentT].id = currentT;
    orders[currentT].N = N;
    if(currentT + N  < NO_OF_ENTRIES)
    {
      entries[currentT + N].addExpiringOrder( &orders[currentT]);
      expiringMap->set(currentT + N);
    }
    else
    {
      expiringBeyondTime = std::min(currentT, expiringBeyondTime);
    }

    result->noOfLiveOrders = currentT - killed + 1;
    int nextEntryToExpire = expiringMap->firstPresent();
    if(nextEntryToExpire != -1)
    {
      OrderBookEntry *nextExpiringEntry = &entries[nextEntryToExpire];
      result->nextToExpire = nextExpiringEntry->expiringHead->id;
    }
    else
    {
      std::cout<<"DEBUG TEST 333"<<std::endl;
      result->nextToExpire = expiringBeyondTime;
    }
    maxNArray[N] = maxNArray[N] + 1;//new order using N
    maxNMap->set(N);
    //maxN = maxNMap->maxPresent();//TODO: revisit this
    //if(maxN < NO_OF_ENTRIES)
    //{
      maxN = maxNMap->maxPresent();//TODO: revisit this
    //}
    result->maxN = maxN;
    Order* expiringNow = entries[currentT].expiringHead;
    while(expiringNow != nullptr)
    {
      expiringAtT++;
      maxNArray[expiringNow->N] = maxNArray[expiringNow->N] - 1;
      if(maxNArray[expiringNow->N] == 0)
      {
        maxNMap->set(expiringNow->N, false);
      }
      expiringNow = expiringNow->expiringNext;
    }

    result->noOfOrdersExpiringAtT = expiringAtT;
    killed += result->noOfOrdersExpiringAtT;
    expiringMap->set(currentT, false);
    ++currentT;

  }

};




constexpr int maxOrderId = 128000000;
//constexpr int maxOrderId = 128;

int main() {


/*
  cpu_set_t  mask;
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);
  CPU_SET(2, &mask);
  int sched_setaffinityResult = sched_setaffinity(0, sizeof(mask), &mask);
*/


    OrderBook<maxOrderId + 1> *ob;
    ob = new OrderBook<maxOrderId + 1>();
    int N;
    ResultAtT result;
    std::cin >> N;
    while(!std::cin.fail())
    {
      ob->addOrder(N, &result);
      std::cout<<result.nextToExpire<<" "<<result.maxN<<" "<<result.noOfLiveOrders<<std::endl;
      std::cout<<result.noOfOrdersExpiringAtT<<std::endl;
      std::cin >> N;
    }
    delete ob;
    return 0;
}
