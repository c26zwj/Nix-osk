cmake_minimum_required(VERSION 3.20)
project(orbit-osk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(LayerShellQt REQUIRED)

qt_add_executable(orbit-osk src/main.cpp)
target_link_libraries(orbit-osk PRIVATE Qt6::Widgets LayerShellQt::Interface)

install(TARGETS orbit-osk RUNTIME DESTINATION bin)
install(FILES packaging/orbit-osk.desktop DESTINATION share/applications)
