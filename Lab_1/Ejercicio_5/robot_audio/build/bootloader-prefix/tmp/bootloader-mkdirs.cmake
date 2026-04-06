# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/diegolagos/esp-idf/components/bootloader/subproject"
  "/home/diegolagos/esp_projects/robot_audio/build/bootloader"
  "/home/diegolagos/esp_projects/robot_audio/build/bootloader-prefix"
  "/home/diegolagos/esp_projects/robot_audio/build/bootloader-prefix/tmp"
  "/home/diegolagos/esp_projects/robot_audio/build/bootloader-prefix/src/bootloader-stamp"
  "/home/diegolagos/esp_projects/robot_audio/build/bootloader-prefix/src"
  "/home/diegolagos/esp_projects/robot_audio/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/diegolagos/esp_projects/robot_audio/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/diegolagos/esp_projects/robot_audio/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
