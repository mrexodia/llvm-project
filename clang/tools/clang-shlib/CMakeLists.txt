# Building libclang-cpp.so fails if LLVM_ENABLE_PIC=Off.  The MSVC dllify
# implementation is a post-link conversion of static archives and does not need
# PIC.
if (NOT LLVM_ENABLE_PIC AND NOT LLVM_USE_MSVC_DLLIFY)
  return()
endif()

get_property(clang_libs GLOBAL PROPERTY CLANG_STATIC_LIBS)
list(REMOVE_ITEM clang_libs clang-repl)

if (CLANG_LINK_CLANG_DYLIB)
  set(INSTALL_WITH_TOOLCHAIN INSTALL_WITH_TOOLCHAIN)
endif()

if(LLVM_USE_MSVC_DLLIFY)
  if(NOT CMAKE_C_COMPILER OR CMAKE_C_COMPILER MATCHES "NOTFOUND")
    message(FATAL_ERROR "CMAKE_C_COMPILER is required for the MSVC clang-cpp.dll generator")
  endif()
  if(NOT CMAKE_LINKER OR CMAKE_LINKER MATCHES "NOTFOUND")
    message(FATAL_ERROR "CMAKE_LINKER is required for the MSVC clang-cpp.dll generator")
  endif()
  if(NOT CMAKE_AR OR CMAKE_AR MATCHES "NOTFOUND")
    message(FATAL_ERROR "CMAKE_AR is required for the MSVC clang-cpp.dll generator")
  endif()
  if(NOT CMAKE_NM OR CMAKE_NM MATCHES "NOTFOUND")
    message(FATAL_ERROR "CMAKE_NM is required for the MSVC clang-cpp.dll generator")
  endif()

  add_clang_library(clang-cpp
                    SHARED
                    ${INSTALL_WITH_TOOLCHAIN}
                    clang-shlib.cpp)
  add_dependencies(clang-cpp LLVM ${clang_libs})

  set(CLANG_MSVC_DLLIFY_STUB_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/dllify-stubs/clang)
  set(CLANG_MSVC_DLLIFY_STUB_LIBS)
  foreach(lib ${clang_libs})
    list(APPEND CLANG_MSVC_DLLIFY_STUB_LIBS ${CLANG_MSVC_DLLIFY_STUB_DIR}/${lib}.lib)
  endforeach()
  target_link_libraries(clang-cpp INTERFACE
    "$<BUILD_INTERFACE:${CLANG_MSVC_DLLIFY_STUB_LIBS}>"
    "$<INSTALL_INTERFACE:${clang_libs}>")

  set(CLANG_MSVC_DLLIFY_LIBSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/clang-cpp-dllify.args)
  set(CLANG_MSVC_DLLIFY_FULL_LIB_NAMES)
  foreach(lib ${clang_libs})
    list(APPEND CLANG_MSVC_DLLIFY_FULL_LIB_NAMES ${LLVM_LIBRARY_OUTPUT_INTDIR}/${lib}.lib)
  endforeach()
  string(REPLACE ";" "\n" CLANG_MSVC_DLLIFY_FILE_CONTENT "${CLANG_MSVC_DLLIFY_FULL_LIB_NAMES}")
  if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
    file(WRITE ${CLANG_MSVC_DLLIFY_LIBSFILE} "${CLANG_MSVC_DLLIFY_FILE_CONTENT}")
  else()
    foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
      string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${CLANG_MSVC_DLLIFY_FILE_CONTENT}")
      file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/clang-cpp-dllify.args "${PER_CONF_CONTENT}")
    endforeach()
  endif()

  llvm_map_components_to_libnames(CLANG_MSVC_DLLIFY_LLVM_LIB_NAMES ${LLVM_DYLIB_COMPONENTS})
  list(REMOVE_ITEM CLANG_MSVC_DLLIFY_LLVM_LIB_NAMES LLVMTableGen)
  list(REMOVE_DUPLICATES CLANG_MSVC_DLLIFY_LLVM_LIB_NAMES)
  set(CLANG_MSVC_DLLIFY_LLVM_DEPS)
  foreach(lib ${CLANG_MSVC_DLLIFY_LLVM_LIB_NAMES})
    list(APPEND CLANG_MSVC_DLLIFY_LLVM_DEPS ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/dllify-stubs/${lib}.lib)
  endforeach()
  set(CLANG_MSVC_DLLIFY_LLVM_DEPSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/clang-cpp-llvm-dllify-deps.args)
  string(REPLACE ";" "\n" CLANG_MSVC_DLLIFY_LLVM_DEPS_CONTENT "${CLANG_MSVC_DLLIFY_LLVM_DEPS}")
  if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
    file(WRITE ${CLANG_MSVC_DLLIFY_LLVM_DEPSFILE} "${CLANG_MSVC_DLLIFY_LLVM_DEPS_CONTENT}")
  else()
    foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
      string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${CLANG_MSVC_DLLIFY_LLVM_DEPS_CONTENT}")
      file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/clang-cpp-llvm-dllify-deps.args "${PER_CONF_CONTENT}")
    endforeach()
  endif()

  set(CLANG_MSVC_DLLIFY_WORK_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tools/clang-shlib/dllify-work)
  add_custom_command(TARGET clang-cpp POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${LLVM_MAIN_SRC_DIR}/tools/llvm-shlib/gen-msvc-dll.py" build
            --libsfile "${CLANG_MSVC_DLLIFY_LIBSFILE}"
            --dll-name clang-cpp
            --resolver-name __clang_resolve
            --no-default-c-api-prefix
            --c-api-prefix clang_
            --dependency-libsfile "${CLANG_MSVC_DLLIFY_LLVM_DEPSFILE}"
            --dependency-lib "${LLVM_LIBRARY_OUTPUT_INTDIR}/LLVM.lib"
            --output-dir "${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}"
            --bin-dir "${LLVM_RUNTIME_OUTPUT_INTDIR}"
            --out-lib-dir "${CLANG_MSVC_DLLIFY_STUB_DIR}"
            --work-dir "${CLANG_MSVC_DLLIFY_WORK_DIR}"
            --cl "${CMAKE_C_COMPILER}"
            --link "${CMAKE_LINKER}"
            --lib-tool "${CMAKE_AR}"
            --nm "${CMAKE_NM}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CLANG_MSVC_DLLIFY_STUB_DIR}/clang-cpp.lib" "${LLVM_LIBRARY_OUTPUT_INTDIR}/clang-cpp.lib"
    BYPRODUCTS "${CLANG_MSVC_DLLIFY_WORK_DIR}/manifest.json" "${CLANG_MSVC_DLLIFY_STUB_DIR}/clang-cpp.lib" ${CLANG_MSVC_DLLIFY_STUB_LIBS}
    COMMENT "Generating resolver-based clang-cpp.dll and clang component stub libraries"
    VERBATIM)

  if(NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
    install(FILES ${CLANG_MSVC_DLLIFY_STUB_LIBS}
            DESTINATION lib${LLVM_LIBDIR_SUFFIX}
            COMPONENT clang-cpp
            OPTIONAL)
    install(FILES "${CLANG_MSVC_DLLIFY_WORK_DIR}/manifest.json"
            DESTINATION share
            RENAME clang-cpp-dllify-manifest.json
            COMPONENT clang-cpp
            OPTIONAL)
  endif()
else()
  foreach (lib ${clang_libs})
    if(XCODE)
      # Xcode doesn't support object libraries, so we have to trick it into
      # linking the static libraries instead.
      list(APPEND _DEPS "-force_load" ${lib})
    else()
      list(APPEND _OBJECTS $<TARGET_OBJECTS:obj.${lib}>)
    endif()
    if (BUILD_SHARED_LIBS)
      # If we are building static libraries, then we don't need to add the static
      # libraries as a dependency, because we are already linking against the
      # individual object files.
      list(APPEND _DEPS $<TARGET_PROPERTY:${lib},INTERFACE_LINK_LIBRARIES>)
    endif()

    # clang libraries are redundant since we are linking all the individual
    # object files into libclang-cpp.so, so filter them out from _DEPS.
    # This avoids problems with LLVM global data when building with
    # BUILD_SHARED_LIBS=ON
    # FIXME: We could use list(FILTER) with cmake >= 3.6
    # FIXME: With cmake >= 3.15 we could use the generator expression
    # $<FILTER:list,INCLUDE|EXCLUDE,regex>
    get_target_property(interface ${lib} LINK_LIBRARIES)
    if (interface)
      foreach(lib ${interface})
        if (NOT ${lib} MATCHES "^clang")
          list(APPEND _DEPS ${lib})
        endif()
      endforeach()
    endif()
  endforeach ()

  if (HAIKU)
    list(APPEND _DEPS network)
  endif()

  add_clang_library(clang-cpp
                    SHARED
                    ${INSTALL_WITH_TOOLCHAIN}
                    clang-shlib.cpp
                    ${_OBJECTS}
                    LINK_LIBS
                    ${_DEPS})

  configure_file(simple_version_script.map.in simple_version_script.map)

  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(clang-cpp PRIVATE LINKER:--version-script,${CMAKE_CURRENT_BINARY_DIR}/simple_version_script.map)
  endif()

  # Optimize function calls for default visibility definitions to avoid PLT and
  # reduce dynamic relocations.
  if (NOT APPLE AND LLVM_LINKER_SUPPORTS_B_SYMBOLIC_FUNCTIONS)
    target_link_options(clang-cpp PRIVATE LINKER:-Bsymbolic-functions)
  endif()
  if (MINGW OR CYGWIN)
    # The clang-cpp DLL is supposed to export all symbols (except for ones
    # that are explicitly hidden). Normally, this is what happens anyway, but
    # if there are symbols that are marked explicitly as dllexport, we'd only
    # export them and nothing else. Therefore, add --export-all-symbols to
    # make sure we export all symbols despite potential dllexports.
    target_link_options(clang-cpp PRIVATE LINKER:--export-all-symbols)
  endif()
endif()
