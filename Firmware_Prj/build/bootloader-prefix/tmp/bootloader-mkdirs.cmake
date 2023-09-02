# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/jeongyeham/esp/esp-idf/components/bootloader/subproject"
  "/home/jeongyeham/Desktop/Hollow_Clock/Firmware_Prj/build/bootloader"
  "/home/jeongyeham/Desktop/Hollow_Clock/Firmware_Prj/build/bootloader-prefix"
  "/home/jeongyeham/Desktop/Hollow_Clock/Firmware_Prj/build/bootloader-prefix/tmp"
  "/home/jeongyeham/Desktop/Hollow_Clock/Firmware_Prj/build/bootloader-prefix/src/bootloader-stamp"
  "/home/jeongyeham/Desktop/Hollow_Clock/Firmware_Prj/build/bootloader-prefix/src"
  "/home/jeongyeham/Desktop/Hollow_Clock/Firmware_Prj/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jeongyeham/Desktop/Hollow_Clock/Firmware_Prj/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/jeongyeham/Desktop/Hollow_Clock/Firmware_Prj/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
