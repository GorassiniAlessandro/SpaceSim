cmake_minimum_required(VERSION 3.16)
project(SpaceSim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(spacesim
    src/main.cpp
    src/app/Application.cpp
    src/core/PhysicsEngine.cpp
    src/render/Renderer2D.cpp
    src/render/Renderer3D.cpp
)

target_include_directories(spacesim PRIVATE include)
