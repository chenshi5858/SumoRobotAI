# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/diegolagos/esp-idf/components/bootloader/subproject"
  "/home/diegolagos/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/robot_audio/build/bootloader"
  "/home/diegolagos/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/robot_audio/build/bootloader-prefix"
  "/home/diegolagos/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/robot_audio/build/bootloader-prefix/tmp"
  "/home/diegolagos/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/robot_audio/build/bootloader-prefix/src/bootloader-stamp"
  "/home/diegolagos/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/robot_audio/build/bootloader-prefix/src"
  "/home/diegolagos/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/robot_audio/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/diegolagos/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/robot_audio/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/diegolagos/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/robot_audio/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
