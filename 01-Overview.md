# Overview

Since a stack for digital communication is no longer something special today, I decided to make the source code for a Hart Slave and a Hart Master available in a Github repository.

The special thing about my implementation is the way it is done and an application developer can only see that in details of the source code and the configuration of the development environment.

The Hart protocol itself and the interfaces to the application are written in C++. I placed great importance on storing all functions and declarations that are independent of a slave or master in common directories.

In the version setting 8.0E the 'E' stands for Experimental. 

## Interfaces

The slave or the master each have two interfaces. One connects the communication driver with the application, the other must create the bridge to the hardware and platform-specific implementation in the system environment. This is also called HAL for Hardware Abstraction Layer.

## Test Implementation

For a universal option for testing (and also for development), I used a Windows DLL for the platform frame of the master and the slave, each of which is visualized with a C# app.

## Hart IP (Internet Protocol)

One of the additional features of my implementation is that it also works with Hart IP.

## Common Sources

About 50 % of the source codes for slave and master are shared by both applications.

Walter Borst, Cuxhaven, 19.1.2025
