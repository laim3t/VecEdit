# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\VecEdit_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\VecEdit_autogen.dir\\ParseCache.txt"
  "VecEdit_autogen"
  )
endif()
