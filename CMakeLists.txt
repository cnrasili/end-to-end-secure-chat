cmake_minimum_required(VERSION 3.21)
project(secure_chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(portaudio CONFIG REQUIRED)

# Shared code used by both the server and the client
add_library(chatcore STATIC
    src/net.cpp
    src/message.cpp
    src/media.cpp
    src/crypto_manager.cpp
)
target_include_directories(chatcore PUBLIC include)
target_link_libraries(chatcore PUBLIC OpenSSL::Crypto)

if(WIN32)
    target_link_libraries(chatcore PUBLIC ws2_32)
    target_compile_definitions(chatcore PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_executable(chat_server src/server.cpp src/server_main.cpp)
target_link_libraries(chat_server PRIVATE chatcore)

add_executable(chat_client src/client.cpp src/client_main.cpp src/audio_recorder.cpp)
target_link_libraries(chat_client PRIVATE chatcore portaudio)
