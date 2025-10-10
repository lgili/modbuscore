
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was Config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include("${CMAKE_CURRENT_LIST_DIR}/modbuscoreTargets.cmake")

set(_modbuscore_impl_target "")
if(TARGET modbuscore::modbus)
    set(_modbuscore_impl_target "modbuscore::modbus")
elseif(TARGET modbuscore::modbuscore)
    set(_modbuscore_impl_target "modbuscore::modbuscore")
endif()

if(_modbuscore_impl_target STREQUAL "")
    message(FATAL_ERROR "ModbusCore package targets were not found")
endif()

if(NOT TARGET modbuscore::modbuscore)
    add_library(modbuscore::modbuscore ALIAS ${_modbuscore_impl_target})
endif()

if(NOT TARGET ModbusCore::modbus)
    add_library(ModbusCore::modbus ALIAS ${_modbuscore_impl_target})
endif()
if(NOT TARGET Modbus::modbus)
    add_library(Modbus::modbus ALIAS ${_modbuscore_impl_target})
endif()

if(TARGET modbuscore::modbuscore)
    set(MODBUSCORE_LIBRARIES modbuscore::modbuscore)
else()
    set(MODBUSCORE_LIBRARIES ${_modbuscore_impl_target})
endif()
set(MODBUSCORE_INCLUDE_DIRS "")
set(MODBUSCORE_LIBRARY_DIRS "")

# Backwards compatibility variables
set(MODBUS_LIBRARIES ${_modbuscore_impl_target})
set(MODBUS_INCLUDE_DIRS "")
set(MODBUS_LIBRARY_DIRS "")

check_required_components(modbuscore)
