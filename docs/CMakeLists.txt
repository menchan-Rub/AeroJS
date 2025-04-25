# CMakeLists.txt for Doxygen documentation
cmake_minimum_required(VERSION 3.14)
project(AeroJSDocs)

find_package(Doxygen REQUIRED)
if(DOXYGEN_FOUND)
  # Configure Doxyfile
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

  # Add custom target to generate documentation
  add_custom_target(doc ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)
else()
  message(WARNING "Doxygen not found. Documentation target will be unavailable.")
endif() 