# Emu-Gators FCEUX [![Build status](https://ci.appveyor.com/api/projects/status/github/TASEmulators/fceux?branch=master&svg=true)](https://ci.appveyor.com/project/zeromus/fceux)

An open source NES Emulator for Windows and Unix that features solid emulation accuracy and state of the art tools for power users. For some reason casual gamers use it too.

Our goal is to customize FCEUX in order to deliver users a more realistic experience when playing games utilizing the Famicom Disk System. Users can use the emulator on Windows or Unix devices. Unix devices have a more complex build process but have the benefit of being able to use physical buttons for certain features of the emulator, mainly Famicom Disk System-specific ones. 

## Production Release Info
* Link to repository - https://github.com/Emu-gators/fceux
* Branch for Linux Build - https://github.com/Emu-gators/fceux/tree/linuxBuild

Note - since the Linux build functions different from the Windows build, if you want to run on Linux make sure to clone the Linux Build branch, not the master.


### Usability
All planned features of the emulator are present, responsive and easy to understand. Both the Windows build and the Linux build are fully finished and thoroughly tested from the Beta Build. The Linux build also has the addition of GPIO input options for unloading ROMs, inserting/ejecting disks, and switching a disk's side. In order to test the Linux build we used Libre's [ROC-RK3328-CC Renegade Board](https://libre.computer/products/roc-rk3328-cc/) flashed with [Raspbian](https://hub.libre.computer/t/raspbian-11-bullseye-for-libre-computer-boards/82).

### Build Quality
Both the Windows build and the Linux build are fully complete, with all content fully integrated. Due to licensing reasons, we cannot include any ROMs or ROM images, but we provide a directory where you can add games you own to autopopulate on launch. The Linux build benefits from being able to use an auto-start feature from the hardware as well as GPIO input options for on screen buttons.

### Features
The features of the Production Release include:

* Custom library for executing lua scripts independent of emulation status
* New C++ functions with Lua bindings for ejecting/inserting, switching and unloading disks
* Redesigned Drag and Drop GUI for loading ROMs
* Auto-populate discovery system for ROM discovery
* Toggleable CRT Filter
* On-screen overlay for disk manipulation procedures
* Hardware implementation testing with ROC-RK3328 Board (Includes an auto-start script and GPIO functionality)

All of these features have been thoroughly tested and works for both the Windows build and the Linux build. For the Linux Build, the ROC-RK3328 interface is used with Raspbian and is able to be interfaced with a mouse, keyboard, joypad and GPIO buttons. For the Windows build it can be interfaced with a mouse, keyboard and joypad. When using the keyboard on the Linux build version, the use of the 'A' key should be treated as the LMB and the 'C' key should be treated as the toggle CRT filter button. The joypad for both builds should strictly be used for playing the ROM outside of the disk manipulation procedures. For the Linux build, the disk manipulation button overlay can be interacted with by using a mouse and the 'A' key or through the use of external pull-down resistor buttons (GPIO). For the Windows build, the disk manipulation button overlay can be interacted only through the mouse peripheral.

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

* auto-start can also be configured on your Linux-based device. This method ensures that the X Window syatem and LXDE desktop environments are initialized before attempting to launch fceux. This is necessary because fceux depends on these to run its GUI. Since this is a device-level operation, we cannot include it in our repository, so instructions will be provided for you to recreate:
	1. Create autostart directory
		mkdir /home/.config/autostart
	2. Add a .desktop file to the autostart directory
		nano /home/.config/autostart/[filename].desktop
	3. Type the following in the file
		[Desktop Entry]
		Type=Application
		Name=FCEUX
		Exec=[path to fceux executable]
FCEUX should now automatically launch on startup after signing in.
Reference: https://learn.sparkfun.com/tutorials/how-to-run-a-raspberry-pi-program-on-startup/all 


* In the "Emugators_Demo.lua" script present in the Linux branch of the repository, there is code is for GPIO implementation. Depending on your device and GPIO pins, you will have to edit the "gpio.lua" file in order to use GPIO properly. It is configured for the ROC-RK3328CC Renegade Board, with Pins 11, 13, and 15 being used for GPIO. If you are using the same device with the same pin setup, you can keep the same code to be able to use the physical buttons for Unloading, Switching, and Inserting/Ejecting disks. Otherwise, you will have to edit the GPIO pins.
