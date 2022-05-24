#include <iostream>
#include <climits>
#include <bitset>
#include <ctime>
#include <ratio>
#include <chrono>
#include <sched.h>


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
constexpr int maxOrderId = 65535;
constexpr int MIN_HEAP_SIZE = 65536;
constexpr int MAX_HEAP_SIZE = 65536;

template <int BITMAP_SIZE>
class ArrayPresenceMap
{
public:
  std::bitset<BITMAP_SIZE> *presence;

  ArrayPresenceMap<BITMAP_SIZE> *parent;
  //aligning to keep small size
  int size;
  int numOfControllers;

  ArrayPresenceMap(int _size): size(_size), parent(nullptr)
  {
    if(size <= BITMAP_SIZE)
    {
      presence = new std::bitset<BITMAP_SIZE>();
    }
    else if(size > BITMAP_SIZE)
    {
      numOfControllers = size/BITMAP_SIZE;
      if(size % BITMAP_SIZE != 0)
      {
        ++numOfControllers;
      }
      presence = new std::bitset<BITMAP_SIZE>[numOfControllers];
      parent = new ArrayPresenceMap<BITMAP_SIZE>(numOfControllers);
    }
  }

  void set(int bit, bool val = true)
  {

    if(parent != nullptr)//TODO: remove the branch
    {
      int presenceNumber = bit/BITMAP_SIZE;
      int offset = bit % BITMAP_SIZE;
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

  int firstPresentAfter(int bit)
  {
    bit++;

    if(parent == nullptr)
    {
      for(int i = bit; i < BITMAP_SIZE; ++i)
      {
        if(presence[0].test(i))
        {
          return i;
        }
      }
      return -1;//TODO: are we going to get here.?
    }


    int presenceNumber = bit/BITMAP_SIZE;
    int offset = bit % BITMAP_SIZE;

    for(int i = offset; i < BITMAP_SIZE; i++)
    {
      if(presence[presenceNumber].test(i))
      {
        return presenceNumber*(BITMAP_SIZE) + i;
      }
    }

    presenceNumber = parent->firstPresentAfter(presenceNumber);

    if(presenceNumber != -1)
    {
      for(int i = 0; i < BITMAP_SIZE; i++)
      {
        if(presence[presenceNumber].test(i))
        {
          return presenceNumber*(BITMAP_SIZE) + i;
        }
      }

    }

    return -1;



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
      for(int i = 0; i < BITMAP_SIZE; ++i)
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

    for(int i = 0; i < BITMAP_SIZE; i++)
    {
      if(presence[parentNumber].test(i))
      {
        return parentNumber*(BITMAP_SIZE) + i;
      }
    }
    return 0;//control wont get here::TODO test
  }

  int maxPresent()
  {
    int i = BITMAP_SIZE;
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

    i = BITMAP_SIZE;
    for(;i--;)
    {
      if(presence[parentNumber].test(i))
      {
        return parentNumber*(BITMAP_SIZE) + i;
      }
    }
    return 0;//control wont get here::TODO test

  }
};

template <int BITMAP_SIZE, int HEAP_SIZE>
class MaxHeap
{
public:
  int currentMax = INT_MIN;
  ArrayPresenceMap<BITMAP_SIZE> ap;
    MaxHeap(): ap(HEAP_SIZE)
    {}

    void insert(int val)
    {
      ap.set(val, true);
      currentMax = std::max(currentMax, val);
    }
    void remove(int val)
    {
      ap.set(val, false);
      if(val == currentMax)
      {
        currentMax = ap.maxPresent();
      }
    }

    int top()
    {
      //return ap.maxPresent();
      return currentMax;
    }
};


template <int BITMAP_SIZE, int HEAP_SIZE>
class MinHeap
{
public:
  int currentMin = INT_MAX;
  ArrayPresenceMap<BITMAP_SIZE> ap;
    MinHeap(): ap(HEAP_SIZE)
    {}

    void insert(int val)
    {
      ap.set(val, true);
      currentMin = std::min(currentMin, val);
    }
    void remove(int val)
    {
      ap.set(val, false);
      if(val == currentMin)
      {
        currentMin = ap.firstPresent();
      }
    }

    int top()
    {
      //return ap.maxPresent();
      return currentMin;
    }

    int firstAfter(int bit)
    {
      //return top();
      //return bit + 1;
      int val = ap.firstPresentAfter(bit);
      if(val != -1)
      {
        return val;
      }
      return top();
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
  Order orders[128000000];// create an order store, prefer stack
  OrderBookEntry entries[NO_OF_ENTRIES];
  int32_t maxNArray[MAX_HEAP_SIZE] = {0};//creating this separately cuz adding it entry class will use extra space due to padding
  int32_t currentT = 0;//not using unsigned cuz they need extra care from compiler and processor, eats up CPU time
  int32_t maxN = INT_MIN;
  int32_t expiringNext = INT_MAX;
  int32_t killed = 0;
  //ArrayPresenceMap<64> *expiringMap=nullptr;
  //ArrayPresenceMap<16> *maxNMap=nullptr;
  MinHeap<16,MIN_HEAP_SIZE> minHeap;
  MaxHeap<16,MAX_HEAP_SIZE> maxHeap;
  OrderBookEntry outBoundEntry;
  int32_t expiringBeyondTime = INT_MAX;
  int32_t expiringNextEntryNumber = INT_MAX;
  OrderBook(){
    //expiringMap = new ArrayPresenceMap<64>(MIN_HEAP_SIZE);
    //maxNMap = new ArrayPresenceMap<16>(MAX_HEAP_SIZE);
  }
  ~OrderBook()
  {
    //delete expiringMap;
    //delete maxNMap;
  }

  void addOrder(int32_t N, ResultAtT* result)//NO return to force RVO
  {
    int expiringAtT = 0;
    //we know currentT is always valid, so doing no checks for validity
    int orderNumber = currentT % NO_OF_ENTRIES;
    int entryNumber = (currentT + N) % NO_OF_ENTRIES;
    expiringNextEntryNumber = std::min(currentT + N, expiringNextEntryNumber);
    orders[currentT].id = currentT;
    orders[currentT].N = N;
    entries[entryNumber].addExpiringOrder( &orders[currentT]);


    minHeap.insert(entryNumber);//expiringMap->set(currentT + N);

    result->noOfLiveOrders = currentT - killed + 1;
    OrderBookEntry *thisOrderEntry = &entries[orderNumber];
    OrderBookEntry *nextExpiringEntry = thisOrderEntry;
    if(expiringNextEntryNumber > currentT + N)
    {
      OrderBookEntry *nextExpiringEntry = &entries[expiringNextEntryNumber%NO_OF_ENTRIES];
      result->nextToExpire = nextExpiringEntry->expiringHead->id;
    }
    else if(nextExpiringEntry->expiringHead != nullptr)
    {
      result->nextToExpire = nextExpiringEntry->expiringHead->id;
    }
    else
    {
      int nextEntryToExpire = minHeap.firstAfter(orderNumber);//minHeap.top();// //expiringMap->firstPresent();
      OrderBookEntry *nextExpiringEntry = &entries[nextEntryToExpire];
      result->nextToExpire = nextExpiringEntry->expiringHead->id;
    }

    maxNArray[N] = maxNArray[N] + 1;//new order using N
    maxHeap.insert(N);//maxNMap->set(N);
    //maxN = maxNMap->maxPresent();//TODO: revisit this
    //if(maxN < NO_OF_ENTRIES)
    //{
    //maxN = maxHeap.top();//maxNMap->maxPresent();//TODO: revisit this
    //}
    result->maxN = maxHeap.top();
    Order* expiringNow = thisOrderEntry->expiringHead;
    while(expiringNow != nullptr)
    {
      expiringAtT++;
      maxNArray[expiringNow->N] = maxNArray[expiringNow->N] - 1;
      if(maxNArray[expiringNow->N] == 0)
      {
        maxHeap.remove(expiringNow->N);//maxNMap->set(expiringNow->N, false);
      }
      expiringNow = expiringNow->expiringNext;
    }
    thisOrderEntry->expiringHead = nullptr;
    thisOrderEntry->expiringTail = nullptr;

    result->noOfOrdersExpiringAtT = expiringAtT;
    killed += result->noOfOrdersExpiringAtT;
    minHeap.remove(orderNumber);//expiringMap->set(currentT, false);
    ++currentT;
  }

};




//constexpr int maxOrderId = 12;

int main() {



  cpu_set_t  mask;
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);
  CPU_SET(2, &mask);
  int sched_setaffinityResult = sched_setaffinity(0, sizeof(mask), &mask);


/*
    ArrayPresenceMap<16> ap(4096);

    ap.set(4, true);
    ap.set(25, true);
    ap.set(38, true);
    ap.set(3333, true);
    std::cout<<" should get 25 = "<<ap.firstPresentAfter(5)<<std::endl;
    std::cout<<" should get 4 = "<<ap.firstPresentAfter(0)<<std::endl;
    std::cout<<" should get 38 = "<<ap.firstPresentAfter(30)<<std::endl;
    std::cout<<" should get 3333 = "<<ap.firstPresentAfter(38)<<std::endl;
    std::cout<<" should get -1 = "<<ap.firstPresentAfter(3333)<<std::endl;
    return 0;
    */
    using namespace std::chrono;
    OrderBook<maxOrderId + 1> *ob;
    ob = new OrderBook<maxOrderId + 1>();
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    int N;
    ResultAtT result;
    std::cin >> N;
    int maxN = N;
    while(!std::cin.fail())
    {
      //ob->addOrder(N, &result);
      std::cout<<result.nextToExpire<<" "<<result.maxN<<" "<<result.noOfLiveOrders<<std::endl;
      std::cout<<result.noOfOrdersExpiringAtT<<std::endl;
      std::cin >> N;
      //maxN = std::max(N, maxN);
    }
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
    std::cout << "It took me " << time_span.count() << " seconds." <<" maxN "<<maxN;
    std::cout << std::endl;
    delete ob;
    return 0;
}
