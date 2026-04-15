# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/chenshi/esp/esp-idf/components/bootloader/subproject"
  "/home/chenshi/esp/projects/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_2/ejercicio1/build/bootloader"
  "/home/chenshi/esp/projects/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_2/ejercicio1/build/bootloader-prefix"
  "/home/chenshi/esp/projects/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_2/ejercicio1/build/bootloader-prefix/tmp"
  "/home/chenshi/esp/projects/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_2/ejercicio1/build/bootloader-prefix/src/bootloader-stamp"
  "/home/chenshi/esp/projects/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_2/ejercicio1/build/bootloader-prefix/src"
  "/home/chenshi/esp/projects/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_2/ejercicio1/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/chenshi/esp/projects/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_2/ejercicio1/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/chenshi/esp/projects/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_2/ejercicio1/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
