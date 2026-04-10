# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Chess2D")

option(CHESS2D_ENABLE_CLAY_UI "Build Clay UI test target and fetch Clay" OFF)
option(CHESS2D_ENABLE_TRACY "Enable Tracy instrumentation" ON)

if (CHESS2D_ENABLE_TRACY)
  include(FetchContent)
  FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.13.1
  )
  FetchContent_MakeAvailable(tracy)

  if (TARGET TracyClient AND NOT TARGET Tracy::TracyClient)
    add_library(Tracy::TracyClient ALIAS TracyClient)
  endif()

  if (NOT TARGET Tracy::TracyClient)
    message(WARNING "Tracy requested but Tracy client target was not created. Build will continue without Tracy instrumentation.")
    set(CHESS2D_ENABLE_TRACY OFF)
  endif()
endif()

# Include sub-projects.
add_subdirectory ("src")
add_subdirectory ("tests")
