#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "modbus::modbus" for configuration "Release"
set_property(TARGET modbus::modbus APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(modbus::modbus PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmodbus.a"
  )

list(APPEND _cmake_import_check_targets modbus::modbus )
list(APPEND _cmake_import_check_files_for_modbus::modbus "${_IMPORT_PREFIX}/lib/libmodbus.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
