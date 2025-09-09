# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 指定自定义组件目录（相对路径或绝对路径）
set(EXTRA_COMPONENT_DIRS 
    "./components/oled"        # OLED组件目录
    "./components/mic_inmp441" # INMP441麦克风组件目录
    "./components/wifi_module" # wifi组件目录
)

project(project-name)
