COMP3000 Final Project - ReadMe.txt
Adrianna Chang and Britta Evans-Fenton

Preamble:

Project title: Comparing Scheduling Algorithms using Elevator Scheduling Modules in Linux
Authors: Adrianna Chang & Britta Evans-Fenton
Date: April 13, 2018
Project description: Each module simulates an elevator system that uses a specific algorithm.
The algorithms used include: round robin, first come first served (FCFS), and shortest distance first (SDF).
The modules use kernel threads to keep the elevator system "running" while passenger requests come in via
system call writes.
The modules were developed under Linux Kernel 4.4 (Ubuntu 16.04).

Included files:
- round_robin.c => Source code for module that simulates elevator system using round robin algorithm
- fcfs.c => Source code for module that simulates elevator system using fcfs algorithm
- sdf.c => Source code for module that simulates elevator system using sdf algorithm
- test_code.c => Code for testing the modules

Instructions to Build Modules:
Enter the folder with the module source code, then enter the following commands:
> make clean
> make
> sudo insmod <module_name.ko>
> sudo chmod go+rw /dev/<module_name>

Example:
> make clean
> make
> sudo insmod round_robin.ko
> sudo chmod go+rw /dev/round_robin

The number of floors and the capacity of the elevator can be modified in the modules by changing the macros NUM_FLOORS
and ELEVATOR_CAPACITY. If changes are made to the module, it must be rebuilt before it can be tested.

Instructions to Test:
Test code is contained within test_code.c. NUM_PASSENGERS can be modified to specify the number of passenger requests
generated, and NUM_FLOORS can be modified to specify the number of floors the elevator has.
* NOTE: The module being tested must have a matching NUM_FLOORS.
* NOTE: Line 22 in test_code.c (fd = open("/dev/<module_name>", O_RDWR);) must be modified to match the module being tested.
For example, if round robin is being tested then (fd = open("/dev/round_robin", O_RDWR);) must be used.

Compile the test code like this:
> gcc -o test test_code.c
Run the test code like this:
> ./test

Observe the behaviour of the elevator:
Statements are printed to the kernel ring buffer, indicating when passengers and being picked up and dropped off and
what floor the elevator is at at any given time. To see these messages, use:
> dmesg | tail -50 // Will show the last 50

When the algorithm has finished, the time it took to complete will be logged to the ring buffer. Use the previous
command shown to view it.

The module must be removed and then reinserted in order to test again. Use the following command to remove it:
> sudo rmmod <module_name>

Example:
> sudo rmmod round_robin
