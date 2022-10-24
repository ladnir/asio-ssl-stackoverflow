

cmake_minimum_required(VERSION 3.20)


option(Boost_USE_MULTITHREADED "mt boost" ON)
option(Boost_USE_STATIC_LIBS "static boost" ON)

message("\n\nOption: Boost_USE_MULTITHREADED=${Boost_USE_MULTITHREADED}")
message("Option: Boost_USE_STATIC_LIBS=${Boost_USE_STATIC_LIBS}")
message("Option: CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}\n\n")

find_package(Boost COMPONENTS system thread REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(mm main.cpp)
target_link_libraries(mm PUBLIC Boost::boost Boost::system Boost::thread OpenSSL::SSL)