# GFOB

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
