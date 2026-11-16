cmake_minimum_required(VERSION 3.16)
project(rov_sim LANGUAGES C CXX)

include(FetchContent)

# MAVLink is header-only. Pin the maintained MAVLink 2 branch to a commit so
# builds do not depend on a system-wide installation or a moving branch head.
FetchContent_Declare(
    mavlink
    GIT_REPOSITORY https://github.com/mavlink/c_library_v2.git
    GIT_TAG        699a2b0df1b259a962a01af788e8306ee8d016d8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(mavlink)

add_executable(rov_sim rovsim.cpp)
target_compile_features(rov_sim PRIVATE cxx_std_17)

# Keep the source's <mavlink/common/mavlink.h> include while MAVLink's
# upstream repository stores that header at <common/mavlink.h>.
set(MAVLINK_INCLUDE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/mavlink-include")
file(MAKE_DIRECTORY "${MAVLINK_INCLUDE_ROOT}/mavlink")
file(CREATE_LINK "${mavlink_SOURCE_DIR}/common" "${MAVLINK_INCLUDE_ROOT}/mavlink/common" SYMBOLIC)
file(GLOB MAVLINK_ROOT_HEADERS "${mavlink_SOURCE_DIR}/*.h")
foreach(header IN LISTS MAVLINK_ROOT_HEADERS)
    get_filename_component(header_name "${header}" NAME)
    configure_file("${header}" "${MAVLINK_INCLUDE_ROOT}/mavlink/${header_name}" COPYONLY)
endforeach()
target_include_directories(rov_sim PRIVATE "${MAVLINK_INCLUDE_ROOT}")
