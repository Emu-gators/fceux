# Emu-Gators FCEUX [![Build status](https://ci.appveyor.com/api/projects/status/github/TASEmulators/fceux?branch=master&svg=true)](https://ci.appveyor.com/project/zeromus/fceux)

An open source NES Emulator for Windows and Unix that features solid emulation accuracy and state of the art tools for power users. For some reason casual gamers use it too.

Our goal is to customize FCEUX in order to deliver users a more realistic experience when playing games utilizing the Famicom Disk System. Users can use the emulator on Windows or Unix devices. Unix devices have a more complex build process but have the benefit of being able to use physical buttons for certain features of the emulator, mainly Famicom Disk System-specific ones. 

## Release Candidate Info
* Link to repository - https://github.com/Emu-gators/fceux

* All major elements of the project are usable, with no bugs resulting in critical failure or loss of information present.
### Usability
All planned features of the emulator are present, responsive and easy to understand. The Windows build is finished and thoroughly tested from the Beta Build. The Linux build is also usable, but we have not been able to fully test the GPIO functionality or do in-depth stress testing on the GUI. In order to test the Linux build we have been using Libre's RC-RK3328-CC Renegade Board flashed with Raspbian. Currently this rendition is stable but has yet been fully tested.

### Build Quality
The Windows build is fully complete, with all content fully integrated. Due to licensing reasons, we cannot include any ROMs or ROM images, but we provide a directory where you can add games you own to autopopulate on launch. 

The Linux build is almost complete, bar minor, known bugs stemming from differences in drivers in the Windows and Linux builds of FCEUX. The GUI has some visual errors, and sound effects in the menu are not handled the same way they are in Windows. However, all other functionality is stable, and the Linux build benefits from being able to use an auto-start feature from the hardware as well as GPIO input options for on screen buttons.  

## Build Instructions
### Windows Build Instructions
The recommended way to build this repository is to clone and build it directly in Visual Studio. If you choose to do this, you will also need the Windows XP toolset. If it is not installed, go to "Tools" > "Get Tools and Features". Select "Individual Components" and then install "C++ Windows XP Support for VS 2017 (v141) tools". These solution files will compile FCEUX and some included libraries for full functionality.

You will also need to have "lua51.dll" in your system environment variables in order to be able to run Lua scripts. This can be done by downloading the 32-bit version of Lua 5.1.5 from the following link - 

https://sourceforge.net/projects/luabinaries/files/5.1.5/Tools%20Executables/lua-5.1.5_Win32_bin.zip/download

Once it is downloaded, unzip the files to a new folder somewhere on your computer. We recommend placing it in "Program Files". Next you will need to copy the path of this folder to your system environment variables (if placed in Program Files, the path will be something like "C:\Program Files\Lua"). Next, right-click the Windows icon in the bottom-left corner of your screen, and select "System". On this page, scroll down to the "Related settings" heading and select "Advanced system settings". On this page, click the "Environment Variables..." button. In this window, scroll down in the box labeled "System variables" until you reach "Path". Double click this line and you will see another window pop up, listing all your environment variables. Here press "New", and you will see another line get added. On this line, paste in the path of the Lua folder created earlier, and press "OK. Then press "OK" on the previous window, after which you will press "OK" on the intial window and the setup will be finished.

You will also have to download the GD library for Lua, which can be found at the following link - 

https://sourceforge.net/projects/lua-gd/files/lua-gd/lua-gd-2.0.33r2%20(for%20Lua%205.1)/

Finally, in order to be able to run games utilizing the Famicom Disk System, which is the entire basis of our project, you will need to have the "disksys.rom" BIOS in the same directory as your executable fceux build. This can be found rather easily online, but the following link provides a version that can be opened and extracted using the 7-Zip Console - 

https://www.mediafire.com/file/35cpj3ghyxeq34o/FamicomDiskSystemBIOS.rar/file

After building, in your cloned repository, navigate to the folder where your executable is found. If you built in Debug mode, it should be in "fceux/vc/vc14_bin_Debug". Then, simply copy/extract the "disksys.rom" file, as well as the GD library downloaded previously into this directory, and you're all set to play Famicom Disk System games.

For USB controller support, simply plug your controller into your device before the emulator is launched. Then, you will have to map the control inside FCEUX in order to achieve the proper buttons. (Note: due to issues with controller drivers, this may not be possible if you are running Windows 11). This is done by navigating to Config -> Input. Ensure "Port 1" is set to Gamepad, and press the corresponding "Configure" button. Here, for Virtual Gamepad 1. click each button, other than Turbo B & Turbo A, with your mouse & then press the corresponding button on your controller. Repeat this process with the same controller for Virtual Gamepad 3 as well. Then, press "Close", followed by another "Close" to navigate back to the emulator.

### Linux Build Instructions
The compilation instructions for Linux builds are copied here for your convinience. The first portion is the generic instructions for the base version of FCEUX, followed by specific instructions for the EmuGators FCEUX build.

We used a ROC-RK3328-CC computer board to test our Linux Build, however, compilation should be the same on any Linux-based device.

-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
1 - Requirements
----------------
* sdl2  - Version >= 2.0  (sdl2 >= 2.8 recommended)
* cmake - Required to build fceux. (cmake >= 3.8 recommended)
* qt5 OR qt6  - (qt version >= 5.11 recommended)
	- Required Qt Modules: Widgets and OpenGL
	- Optional Qt Modules: Help
* liblua5.1 (optional) - Will statically link internally if the system cannot provide this.
* libx264 (optional) - H.264 video encoder for avi recording (recommended)
* libx265 (optional) - H.265 video encoder for avi recording (recommended)
* ffmpeg libraries (optional) - for avi recording (recommended)
*	- ffmpeg libraries used: libavcodec  libavformat  libavutil  libswresample  libswscale
* minizip  
* zlib  
* openGL
* c++ compiler -- you may use g++ from gcc or clang++ from llvm or msvc 2019

2 - Installation
----------------
The old scons build system is no longer supported.
Fceux can be compiled and built using the cmake build system.  To compile, run:

	mkdir build; cd build;
   cmake  -DCMAKE_INSTALL_PREFIX=/usr  -DCMAKE_BUILD_TYPE=Release  ..    # For a release build

To build a binary with debug information included in it:
   cmake  -DCMAKE_INSTALL_PREFIX=/usr  -DCMAKE_BUILD_TYPE=Debug    ..    

The Qt version of the GUI builds by default and this is the preferred GUI for use. 
For those who must have GTK/Gnome style, there is a GTK style plugin for Qt that 
can be installed to the OS and selected for use via the GUI.
The previous GTK version of the GUI is now retired and has been removed from the build.

The Qt version GUI by far exceeds the old GTK gui in capability and performance.
The Qt GUI can build/link against both Qt5 and Qt6. To enable building against Qt6, use -DQT6=1 argument:
   cmake  -DCMAKE_INSTALL_PREFIX=/usr  -DQT6=1  -DCMAKE_BUILD_TYPE=Debug    ..    

To do the actual compiling:
   make

To compile faster with multiple processes in parallel:
   make -j `nproc`
	
After a sucessful compilation, the fceux binary will be generated to 
./build/src/fceux .  You can install fceux to your system with the following command:

	sudo make install

By default cmake will use an installation prefix of /usr/local.
The recommended installation prefix is /usr (this is where the installed fceux.desktop file expects everything to be)

You can choose to install the lua scripts (located in output/luaScripts) to a directory of your choosing:

	cp -R output/luaScripts /usr/local/some/directory/that/i/will/find/later

If you would like to do a full clean build a 'make clean' like function, you can do either of following:

   make clean    # from inside build directory

OR:
	Delete build directory and start over at initial cmake step:
   rm -rf build;
   
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------  
In addition, you will also need to download the following lua packages:
* lua-gd, link present above
* lua-periphery, https://github.com/vsergeev/lua-periphery

* auto-start can also be configured on your Linux-based device. Since this is a device-level operation, we cannot include it in our repository, so instructions will be provided for you to recreate.

* In the "Emugators_Demo.lua" script, you will find certain portions of code commented out with notes mentioning that the code is for GPIO implementation. Depending on your device and GPIO pins, you will have to edit this in order to use it properly. It is configured for the ROC-RK3328CC Renegade Board, with Pins 11, 13, and 15 being used for GPIO. If you are using the same device with the same pin setup, you can un-comment the appropriate code to be able to use the physical buttons for Unloading, Switching, and Inserting/Ejecting disks.

