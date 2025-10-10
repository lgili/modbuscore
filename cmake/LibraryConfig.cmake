# Target
add_library(${LIBRARY_NAME}
  ${SOURCES}
  ${HEADERS_PUBLIC}
  ${HEADERS_PRIVATE}
)


# Alias:
#   - Foo::foo alias of foo
add_library(${PROJECT_NAME}::${LIBRARY_NAME} ALIAS ${LIBRARY_NAME})
if(DEFINED PACKAGE_NAME)
  set(_package_namespace "${PACKAGE_NAME}")
else()
  set(_package_namespace "modbuscore")
endif()
if(NOT TARGET ${_package_namespace}::modbuscore)
  add_library(${_package_namespace}::modbuscore ALIAS ${LIBRARY_NAME})
endif()
if(NOT TARGET Modbus::modbus)
  add_library(Modbus::modbus ALIAS ${LIBRARY_NAME})
endif()
unset(_package_namespace)

if(MODBUS_FORCE_C99)
  target_compile_features(${LIBRARY_NAME} PUBLIC c_std_99)
else()
  target_compile_features(${LIBRARY_NAME} PUBLIC c_std_11)
endif()

if(MODBUS_WARN_FLAGS)
  target_compile_options(${LIBRARY_NAME} PRIVATE ${MODBUS_WARN_FLAGS})
endif()

# Add definitions for targets
# Values:
#   - Debug  : -DFOO_DEBUG=1
#   - Release: -DFOO_DEBUG=0
#   - others : -DFOO_DEBUG=0
target_compile_definitions(${LIBRARY_NAME} PUBLIC
  "${PROJECT_NAME_UPPERCASE}_DEBUG=$<CONFIG:Debug>")

# Global includes. Used by all targets
# Note:
#   - header can be included by C++ code `#include <foo/foo.h>`
#   - header location in project: ${CMAKE_CURRENT_BINARY_DIR}/generated_headers
target_include_directories(
  ${LIBRARY_NAME} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${GENERATED_HEADERS_DIR}>"
    "$<INSTALL_INTERFACE:include>"
)

# Targets:
#   - <prefix>/lib/libfoo.a
#   - header location after install: <prefix>/foo/foo.h
#   - headers can be included by C++ code `#include <foo/foo.h>`
if(MODBUS_INSTALL)
  install(
      TARGETS              "${LIBRARY_NAME}"
      EXPORT               "${TARGETS_EXPORT_NAME}"
      LIBRARY DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
      ARCHIVE DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
      RUNTIME DESTINATION  "${CMAKE_INSTALL_BINDIR}"
      INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )

  # Headers:
  #   - foo/*.h -> <prefix>/include/foo/*.h
  install(
      FILES ${HEADERS_PUBLIC}
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_FOLDER}"
  )

  # Headers:
  #   - generated_headers/foo/version.h -> <prefix>/include/foo/version.h
  install(
      FILES       "${GENERATED_HEADERS_DIR}/${LIBRARY_FOLDER}/version_config.h"
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_FOLDER}"
  )

  # Config
  #   - <prefix>/lib/cmake/Foo/FooConfig.cmake
  #   - <prefix>/lib/cmake/Foo/FooConfigVersion.cmake
  install(
      FILES       "${PROJECT_CONFIG_FILE}"
      DESTINATION "${CONFIG_INSTALL_DIR}"
  )
  if(EXISTS "${COMPAT_PROJECT_CONFIG_FILE}")
    install(
        FILES       "${COMPAT_PROJECT_CONFIG_FILE}"
        DESTINATION "${CONFIG_INSTALL_DIR}"
        RENAME      "ModbusCoreConfig.cmake"
    )
  endif()
  install(
      FILES       "${VERSION_CONFIG_FILE}"
      DESTINATION "${CONFIG_INSTALL_DIR}"
  )
  install(
      FILES       "${VERSION_CONFIG_FILE}"
      DESTINATION "${CONFIG_INSTALL_DIR}"
      RENAME      "ModbusCoreConfigVersion.cmake"
  )

  if(PKG_CONFIG_FILE)
    install(
      FILES "${PKG_CONFIG_FILE}"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )
  endif()

  # Config
  #   - <prefix>/lib/cmake/Foo/FooTargets.cmake
  install(
    EXPORT      "${TARGETS_EXPORT_NAME}"
    FILE        "${TARGETS_EXPORT_NAME}.cmake"
    DESTINATION "${CONFIG_INSTALL_DIR}"
    NAMESPACE   "${PACKAGE_NAME}::"
  )
endif()
