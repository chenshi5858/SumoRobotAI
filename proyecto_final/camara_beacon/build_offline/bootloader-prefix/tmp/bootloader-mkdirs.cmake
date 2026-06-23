# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/chenshi/esp/idf/esp-idf/components/bootloader/subproject"
  "/home/chenshi/embebidos_repo/Rep_SE_202601_grupo_Diego_Santiago_Chen/proyecto_final/camara_beacon/build_offline/bootloader"
  "/home/chenshi/embebidos_repo/Rep_SE_202601_grupo_Diego_Santiago_Chen/proyecto_final/camara_beacon/build_offline/bootloader-prefix"
  "/home/chenshi/embebidos_repo/Rep_SE_202601_grupo_Diego_Santiago_Chen/proyecto_final/camara_beacon/build_offline/bootloader-prefix/tmp"
  "/home/chenshi/embebidos_repo/Rep_SE_202601_grupo_Diego_Santiago_Chen/proyecto_final/camara_beacon/build_offline/bootloader-prefix/src/bootloader-stamp"
  "/home/chenshi/embebidos_repo/Rep_SE_202601_grupo_Diego_Santiago_Chen/proyecto_final/camara_beacon/build_offline/bootloader-prefix/src"
  "/home/chenshi/embebidos_repo/Rep_SE_202601_grupo_Diego_Santiago_Chen/proyecto_final/camara_beacon/build_offline/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/chenshi/embebidos_repo/Rep_SE_202601_grupo_Diego_Santiago_Chen/proyecto_final/camara_beacon/build_offline/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/chenshi/embebidos_repo/Rep_SE_202601_grupo_Diego_Santiago_Chen/proyecto_final/camara_beacon/build_offline/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
