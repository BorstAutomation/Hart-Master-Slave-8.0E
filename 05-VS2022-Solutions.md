# VS 2022 Solutions

This package contains two solutions for Visual Studio 2022, one for a Hart Master and one for a Hart Slave.

## Hart Master

The solution can be found in the following path: **.\02-Master\CppHartMaster-8.0E.sln**.

### Windows DLL (C++)

The solution has two projects. The project with the source code for the Hart Master is located at the following path: **.\02-Master\02-Code\02-Specific\01-WinDLL\HartMasterDLL.vcxproj**.

The project is specific because a Windows library (DLL) is created from the sources of the master in which the 'device' works. To speak of a simulation here is actually wrong, because the core of the master runs in a Windows thread at millisecond intervals and behaves exactly as the implementation on an embedded system would.

Regarding the source code paths, it should be noted that the modules that are included by master and slave can be found in a special area: **.\01-Master&Slave\01-C++**.

### Windows EXE (C#)

A Windows DLL is of course not able to run on its own. To use it, you need an executable application. The C# sources and the project for this test client can be found at the following location:

**.\02-Master\03-Test\01-Windows\02-Apps\01-TestClientMaster\BaTestHartMaster.csproj**.

The sources for the test clients also contain a whole series of modules that are shared between master and slave. These are located in the folder: **.\01-Master&Slave\02-C#**.

To make things less complicated, it is best if the DLL and the executable are in the same path. I called this special directory DebugBench: **.\02-Master\03-Test\01-Windows\03-DebugBench**.

## Hart Slave

Listing all of this again for the slave would be like carrying coals to Newcastle, since the slave test client is structured in the same way as the one for the master.

Walter Borst, Cuxhaven, 19.1.2025

