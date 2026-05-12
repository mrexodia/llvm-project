if(NOT LLVM_USE_MSVC_DLLIFY)
  message(FATAL_ERROR "lld.dll generation is currently implemented only for the MSVC dllify LLVM.dll mode")
endif()

set(LLD_MSVC_DLLIFY_LIB_NAMES
  lldCommon
  lldCOFF
  lldELF
  lldMachO
  lldMinGW
  lldWasm
  )

add_lld_library(lld-shlib
  SHARED
  OUTPUT_NAME lld
  lld-shlib.cpp
  )
add_dependencies(lld-shlib LLVM ${LLD_MSVC_DLLIFY_LIB_NAMES})

target_compile_definitions(lld-shlib INTERFACE LLD_BUILD_STATIC)

set(LLD_MSVC_DLLIFY_STUB_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/dllify-stubs/lld)
set(LLD_MSVC_DLLIFY_STUB_LIBS)
foreach(lib ${LLD_MSVC_DLLIFY_LIB_NAMES})
  list(APPEND LLD_MSVC_DLLIFY_STUB_LIBS ${LLD_MSVC_DLLIFY_STUB_DIR}/${lib}.lib)
endforeach()
target_link_libraries(lld-shlib INTERFACE
  "$<BUILD_INTERFACE:${LLD_MSVC_DLLIFY_STUB_LIBS}>"
  "$<INSTALL_INTERFACE:${LLD_MSVC_DLLIFY_LIB_NAMES}>")

set(LLD_MSVC_DLLIFY_LIBSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lld-dllify.args)
set(LLD_MSVC_DLLIFY_FULL_LIB_NAMES)
foreach(lib ${LLD_MSVC_DLLIFY_LIB_NAMES})
  list(APPEND LLD_MSVC_DLLIFY_FULL_LIB_NAMES ${LLVM_LIBRARY_OUTPUT_INTDIR}/${lib}.lib)
endforeach()
string(REPLACE ";" "\n" LLD_MSVC_DLLIFY_FILE_CONTENT "${LLD_MSVC_DLLIFY_FULL_LIB_NAMES}")
if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
  file(WRITE ${LLD_MSVC_DLLIFY_LIBSFILE} "${LLD_MSVC_DLLIFY_FILE_CONTENT}")
else()
  foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
    string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${LLD_MSVC_DLLIFY_FILE_CONTENT}")
    file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/lld-dllify.args "${PER_CONF_CONTENT}")
  endforeach()
endif()

llvm_map_components_to_libnames(LLD_MSVC_DLLIFY_LLVM_LIB_NAMES ${LLVM_DYLIB_COMPONENTS})
list(REMOVE_ITEM LLD_MSVC_DLLIFY_LLVM_LIB_NAMES LLVMTableGen)
list(REMOVE_DUPLICATES LLD_MSVC_DLLIFY_LLVM_LIB_NAMES)
set(LLD_MSVC_DLLIFY_LLVM_DEPS)
foreach(lib ${LLD_MSVC_DLLIFY_LLVM_LIB_NAMES})
  list(APPEND LLD_MSVC_DLLIFY_LLVM_DEPS ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/dllify-stubs/${lib}.lib)
endforeach()
set(LLD_MSVC_DLLIFY_LLVM_DEPSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lld-llvm-dllify-deps.args)
string(REPLACE ";" "\n" LLD_MSVC_DLLIFY_LLVM_DEPS_CONTENT "${LLD_MSVC_DLLIFY_LLVM_DEPS}")
if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
  file(WRITE ${LLD_MSVC_DLLIFY_LLVM_DEPSFILE} "${LLD_MSVC_DLLIFY_LLVM_DEPS_CONTENT}")
else()
  foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
    string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${LLD_MSVC_DLLIFY_LLVM_DEPS_CONTENT}")
    file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/lld-llvm-dllify-deps.args "${PER_CONF_CONTENT}")
  endforeach()
endif()

set(LLD_MSVC_DLLIFY_WORK_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tools/lld-shlib/dllify-work)
add_custom_command(TARGET lld-shlib POST_BUILD
  COMMAND "${Python3_EXECUTABLE}" "${LLVM_MAIN_SRC_DIR}/tools/llvm-shlib/gen-msvc-dll.py" build
          --libsfile "${LLD_MSVC_DLLIFY_LIBSFILE}"
          --dll-name lld
          --resolver-name __lld_resolve
          --no-default-c-api-prefix
          --dependency-libsfile "${LLD_MSVC_DLLIFY_LLVM_DEPSFILE}"
          --dependency-lib "${LLVM_LIBRARY_OUTPUT_INTDIR}/LLVM.lib"
          --output-dir "${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}"
          --bin-dir "${LLVM_RUNTIME_OUTPUT_INTDIR}"
          --out-lib-dir "${LLD_MSVC_DLLIFY_STUB_DIR}"
          --work-dir "${LLD_MSVC_DLLIFY_WORK_DIR}"
          --cl "${CMAKE_C_COMPILER}"
          --link "${CMAKE_LINKER}"
          --lib-tool "${CMAKE_AR}"
          --nm "${CMAKE_NM}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LLD_MSVC_DLLIFY_STUB_DIR}/lld.lib" "${LLVM_LIBRARY_OUTPUT_INTDIR}/lld.lib"
  BYPRODUCTS "${LLD_MSVC_DLLIFY_WORK_DIR}/manifest.json" "${LLD_MSVC_DLLIFY_STUB_DIR}/lld.lib" ${LLD_MSVC_DLLIFY_STUB_LIBS}
  COMMENT "Generating resolver-based lld.dll and lld component stub libraries"
  VERBATIM)

if(NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
  install(FILES ${LLD_MSVC_DLLIFY_STUB_LIBS}
          DESTINATION lib${LLVM_LIBDIR_SUFFIX}
          COMPONENT lld-shlib
          OPTIONAL)
  install(FILES "${LLD_MSVC_DLLIFY_WORK_DIR}/manifest.json"
          DESTINATION share
          RENAME lld-dllify-manifest.json
          COMPONENT lld-shlib
          OPTIONAL)
endif()
