# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/yuusha1/esp/idf/esp-idf/components/bootloader/subproject"
  "/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_4/build/bootloader"
  "/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_4/build/bootloader-prefix"
  "/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_4/build/bootloader-prefix/tmp"
  "/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_4/build/bootloader-prefix/src/bootloader-stamp"
  "/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_4/build/bootloader-prefix/src"
  "/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_4/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_4/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_4/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
