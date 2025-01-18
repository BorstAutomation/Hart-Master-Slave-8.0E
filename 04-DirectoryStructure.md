# Directory Structure

## Overview

| Directory             | Description                                                  |
| --------------------- | ------------------------------------------------------------ |
| .\01-Master&Slave     | Items which are common to the master and the slave implementation. |
| .\01-Master&Slave\C++ | C++ modules common to master and slave.                      |
| .\01-Master&Slave\C#  | C# modules common to master and slave.                       |
| .\02-Master           | Implementation of the Hart master.                           |
| .\03-Slave            | Implementation of the Hart slave.                            |
| .\04-Test\Windows     | Contains a dos batch script for starting the master and the slave at once. |

## .\02-Master

 This path contains the C++ sources for the master and a test client implementation in C# code.

| Directory                                           | Description                                                  |
| --------------------------------------------------- | ------------------------------------------------------------ |
| .\02_Master                                         | Contains the VS 2022 solution with the C++ master sources for a device simulation (DLL) and the C# sources for a test client. |
| .\02_Master\01_Docu                                 | Documentation                                                |
| .\02_Master\02_Code                                 | Source codes for the device                                  |
| .\02_Master\02_Code\01_Common                       | Hardware independent C++ sources.                            |
| .\02_Master\02_Code\02_Specific                     | Hardware specific C++ sources                                |
| .\02_Master\02_Code\02_Specific\01_WinDLL           | Sources for a Windows simulation DLL and the project file for VS 2022 |
| .\02_Master\02_Code\02_Specific\01_WinDLL\01_Shell  | User (Test Client) interface of the DLL                      |
| .\02_Master\02_Code\02_Specific\01_WinDLL\\02_OSAL  | Operating System Abstraction Layer                           |
| .\02_Master\02_Code\02_Specific\01_WinDLL\\03_Build | Build output for the compiler                                |
| .\02_Master\02_Code\02_Specific\02_Nrf52832         | Space for a specific embedded project                        |
| .\02_Master\03_Test                                 | Space for test clients                                       |
| .\02_Master\03_Test\01_Windows                      | Space for Windows test packets                               |
| .\02_Master\03_Test\01_Windows\01_Docu              | Documentation                                                |
| .\02_Master\03_Test\01_Windows\02_Apps              | Executable test clients                                      |
| +\01_TestClientMaster                               | Test client for the master (VS 2022 project for a C# application) |
| +\01_TestClientMaster\01_Main                       | Main form of the test client                                 |
| +\01_TestClientMaster\02_Modules                    | Further C# submodules                                        |
| +\01_TestClientMaster\02_Modules\01_Helpers         | Helper routines                                              |
| +\01_TestClientMaster\02_Modules\02_Forms           | Additional Windows forms                                     |
| +\01_TestClientMaster\02_Modules\03_TestClient      | Special test routines                                        |
| +\01_TestClientMaster\02_Modules\04_HartMasterIntf  | C# interface to the C++ DLL                                  |
| +\01_TestClientMaster\03_DebugBench                 | All run time libraries and required executables are copied into this directory. The exe file can be executed in here. |
| +\01_TestClientMaster\04_Results                    | Not yet used.                                                |

## .\03-Slave

The subdirectory for the slave is structured exactly the same as the one for the master. Therefore, it makes no sense to list it all again here.

Walter Borst, Cuxhaven, 19.1.2025
