cmake_minimum_required(VERSION 3.20)

project(Projet1CIR2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SFML_STATIC_LIBRARIES TRUE)

if(MSVC)
    set(SFML_DIR "C:/Program Files/SFML/SFML_3.0.2/lib/cmake/SFML")
    add_compile_options(/utf-8)
endif(MSVC)


find_package(SFML 3 COMPONENTS Window Graphics System REQUIRED)
find_package(Threads REQUIRED)

add_executable(Simulateur
    "Projet/main.cpp"
    "Projet/avion.cpp"
    "Projet/thread.cpp"
    "Projet/avion.hpp"
    "Projet/thread.hpp"
    "Projet/position.cpp" 
    "Projet/parking.cpp" 
    "Projet/twr.cpp" 
    "Projet/app.cpp" 
    "Projet/ccr.cpp"  
    "Projet/communication.cpp" "Projet/sfml.cpp")

target_link_libraries(Simulateur PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System
    Threads::Threads
)

add_custom_command(TARGET Simulateur POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Projet/img"
    "$<TARGET_FILE_DIR:Simulateur>/img"
    COMMENT "Copie du dossier img"
)

add_custom_command(TARGET Simulateur POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/Projet/debut.txt"
    "$<TARGET_FILE_DIR:Simulateur>/debut.txt"
    COMMENT "Copie de debut.txt"
)