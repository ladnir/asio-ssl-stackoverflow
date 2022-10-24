

cmake_minimum_required(VERSION 3.20)
message("CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}\n\n")
option(Boost_USE_MULTITHREADED "mt boost" ON)
option(Boost_USE_STATIC_LIBS "static boost" ON)

find_package(Boost 1.77 COMPONENTS system thread REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(mm main.cpp)
target_link_libraries(mm PUBLIC Boost::boost Boost::system Boost::thread OpenSSL::SSL)