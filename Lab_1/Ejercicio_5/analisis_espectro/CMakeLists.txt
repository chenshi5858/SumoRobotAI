cmake_minimum_required(VERSION 3.22)

if(NOT IDF_TARGET)
    set(IDF_TARGET "esp32s3")
endif()

set(EXTRA_COMPONENT_DIRS "../robot_audio/managed_components/espressif__esp-dsp")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(analisis_espectro)
