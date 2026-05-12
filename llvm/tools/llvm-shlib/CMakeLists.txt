# This tool creates a shared library from the LLVM libraries. Generating this
# library is enabled by setting LLVM_BUILD_LLVM_DYLIB=yes on the CMake
# commandline. By default the shared library only exports the LLVM C API.

set(SOURCES
  libllvm.cpp
  )

if(LLVM_LINK_LLVM_DYLIB AND LLVM_DYLIB_EXPORTED_SYMBOL_FILE)
  message(WARNING "Using LLVM_LINK_LLVM_DYLIB with LLVM_DYLIB_EXPORTED_SYMBOL_FILE may not work. Use at your own risk.")
endif()

if(LLVM_BUILD_LLVM_DYLIB)
  if(MSVC AND NOT LLVM_BUILD_LLVM_DYLIB_VIS AND NOT LLVM_USE_MSVC_DLLIFY)
    message(FATAL_ERROR "Generating libLLVM is not supported on MSVC")
  endif()
  if(ZOS)
    message(FATAL_ERROR "Generating libLLVM is not supported on z/OS")
  endif()

  llvm_map_components_to_libnames(LIB_NAMES ${LLVM_DYLIB_COMPONENTS})

  # Exclude libLLVMTableGen for the following reasons:
  #  - it is only used by internal *-tblgen utilities;
  #  - it pollutes the global options space.
  list(REMOVE_ITEM LIB_NAMES "LLVMTableGen")

  if(LLVM_DYLIB_EXPORTED_SYMBOL_FILE)
    set(LLVM_EXPORTED_SYMBOL_FILE ${LLVM_DYLIB_EXPORTED_SYMBOL_FILE})
    add_custom_target(libLLVMExports DEPENDS ${LLVM_EXPORTED_SYMBOL_FILE})
  endif()

  list(REMOVE_DUPLICATES LIB_NAMES)

  if (LLVM_LINK_LLVM_DYLIB)
    set(INSTALL_WITH_TOOLCHAIN INSTALL_WITH_TOOLCHAIN)
  endif()

  if(LLVM_USE_MSVC_DLLIFY)
    if(LLVM_DYLIB_EXPORTED_SYMBOL_FILE)
      message(FATAL_ERROR "LLVM_DYLIB_EXPORTED_SYMBOL_FILE is not supported by the MSVC LLVM.dll generator")
    endif()
    if(NOT CMAKE_C_COMPILER OR CMAKE_C_COMPILER MATCHES "NOTFOUND")
      message(FATAL_ERROR "CMAKE_C_COMPILER is required for the MSVC LLVM.dll generator")
    endif()
    if(NOT CMAKE_LINKER OR CMAKE_LINKER MATCHES "NOTFOUND")
      message(FATAL_ERROR "CMAKE_LINKER is required for the MSVC LLVM.dll generator")
    endif()
    if(NOT CMAKE_AR OR CMAKE_AR MATCHES "NOTFOUND")
      message(FATAL_ERROR "CMAKE_AR is required for the MSVC LLVM.dll generator")
    endif()
    if(NOT CMAKE_NM OR CMAKE_NM MATCHES "NOTFOUND")
      message(FATAL_ERROR "CMAKE_NM is required for the MSVC LLVM.dll generator")
    endif()

    # Create a normal CMake shared-library target so in-tree tools, install(),
    # exported CMake packages, and $<TARGET_RUNTIME_DLLS> all see an LLVM DLL.
    # The target first links an empty placeholder DLL, then the post-build step
    # replaces LLVM.dll/LLVM.lib and the component archives with resolver stubs
    # generated from the just-built static component libraries.
    add_llvm_library(LLVM SHARED DISABLE_LLVM_LINK_LLVM_DYLIB OUTPUT_NAME LLVM ${INSTALL_WITH_TOOLCHAIN} ${SOURCES})
    add_dependencies(LLVM ${LIB_NAMES})

    set(LLVM_MSVC_DLLIFY_STUB_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/dllify-stubs)
    set(LLVM_MSVC_DLLIFY_STUB_LIBS)
    foreach(lib ${LIB_NAMES})
      list(APPEND LLVM_MSVC_DLLIFY_STUB_LIBS ${LLVM_MSVC_DLLIFY_STUB_DIR}/${lib}.lib)
    endforeach()
    target_link_libraries(LLVM INTERFACE
      "$<BUILD_INTERFACE:${LLVM_MSVC_DLLIFY_STUB_LIBS}>"
      "$<INSTALL_INTERFACE:${LIB_NAMES}>")

    set(LLVM_MSVC_DLLIFY_LIBSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/libllvm-dllify.args)
    set(LLVM_MSVC_DLLIFY_FULL_LIB_NAMES)
    foreach(lib ${LIB_NAMES})
      list(APPEND LLVM_MSVC_DLLIFY_FULL_LIB_NAMES ${LLVM_LIBRARY_DIR}/${lib}.lib)
    endforeach()
    string(REPLACE ";" "\n" LLVM_MSVC_DLLIFY_FILE_CONTENT "${LLVM_MSVC_DLLIFY_FULL_LIB_NAMES}")
    if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
      file(WRITE ${LLVM_MSVC_DLLIFY_LIBSFILE} "${LLVM_MSVC_DLLIFY_FILE_CONTENT}")
    else()
      foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
        string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${LLVM_MSVC_DLLIFY_FILE_CONTENT}")
        file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/libllvm-dllify.args "${PER_CONF_CONTENT}")
      endforeach()
    endif()

    set(LLVM_MSVC_DLLIFY_WORK_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tools/llvm-shlib/dllify-work)
    add_custom_command(TARGET LLVM POST_BUILD
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/gen-msvc-dll.py" build
              --libsfile "${LLVM_MSVC_DLLIFY_LIBSFILE}"
              --output-dir "${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}"
              --bin-dir "${LLVM_TOOLS_BINARY_DIR}"
              --out-lib-dir "${LLVM_MSVC_DLLIFY_STUB_DIR}"
              --work-dir "${LLVM_MSVC_DLLIFY_WORK_DIR}"
              --cl "${CMAKE_C_COMPILER}"
              --link "${CMAKE_LINKER}"
              --lib-tool "${CMAKE_AR}"
              --nm "${CMAKE_NM}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LLVM_MSVC_DLLIFY_STUB_DIR}/LLVM.lib" "${LLVM_LIBRARY_DIR}/LLVM.lib"
      BYPRODUCTS "${LLVM_MSVC_DLLIFY_WORK_DIR}/manifest.json" "${LLVM_MSVC_DLLIFY_STUB_DIR}/LLVM.lib" ${LLVM_MSVC_DLLIFY_STUB_LIBS}
      COMMENT "Generating resolver-based LLVM.dll and LLVM component stub libraries"
      VERBATIM)

    if(NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
      install(FILES ${LLVM_MSVC_DLLIFY_STUB_LIBS}
              DESTINATION lib${LLVM_LIBDIR_SUFFIX}
              COMPONENT LLVM
              OPTIONAL)
      install(FILES "${LLVM_MSVC_DLLIFY_WORK_DIR}/manifest.json"
              DESTINATION share
              RENAME llvm-dllify-manifest.json
              COMPONENT LLVM
              OPTIONAL)
    endif()
  else()
    if (WIN32 OR CYGWIN)
      add_llvm_library(LLVM SHARED DISABLE_LLVM_LINK_LLVM_DYLIB SONAME ${INSTALL_WITH_TOOLCHAIN} ${SOURCES})
    else()
      add_llvm_library(LLVM SHARED DISABLE_LLVM_LINK_LLVM_DYLIB OUTPUT_NAME LLVM ${INSTALL_WITH_TOOLCHAIN} ${SOURCES})
      # Add symlink for backwards compatibility with old library name
      llvm_install_library_symlink(LLVM-${LLVM_VERSION_MAJOR}${LLVM_VERSION_SUFFIX} $<TARGET_FILE_NAME:LLVM> SHARED FULL_DEST COMPONENT LLVM)
    endif()

    if (MINGW OR CYGWIN)
      # The LLVM DLL is supposed to export all symbols (except for ones
      # that are explicitly hidden). Normally, this is what happens anyway, but
      # if there are symbols that are marked explicitly as dllexport, we'd only
      # export them and nothing else. Therefore, add --export-all-symbols to
      # make sure we export all symbols despite potential dllexports.
      target_link_options(LLVM PRIVATE LINKER:--export-all-symbols)
    endif()

    if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
      set(LIB_NAMES -Wl,-all_load ${LIB_NAMES})
    else()
      if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
        configure_file(
          ${CMAKE_CURRENT_SOURCE_DIR}/simple_version_script.map.in
          ${LLVM_LIBRARY_DIR}/tools/llvm-shlib/simple_version_script.map)
      else()
        foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
          # Replace the special string with a per config directory.
          string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_LIBRARY_DIR ${LLVM_LIBRARY_DIR})
          configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/simple_version_script.map.in
            ${PER_CONF_LIBRARY_DIR}/tools/llvm-shlib/simple_version_script.map)
        endforeach()
      endif()
      if(MSVC)
        target_link_directories(LLVM PRIVATE ${LLVM_LIBRARY_DIR})
        foreach(library ${LIB_NAMES})
          target_link_options(LLVM PRIVATE /WHOLEARCHIVE:${library}.lib)
        endforeach()
      else()
        # GNU ld doesn't resolve symbols in the version script.
        set(LIB_NAMES -Wl,--whole-archive ${LIB_NAMES} -Wl,--no-whole-archive)
        if (NOT LLVM_LINKER_IS_SOLARISLD AND NOT MINGW AND NOT CYGWIN)
          # Solaris ld does not accept global: *; so there is no way to version *all* global symbols
          set(LIB_NAMES -Wl,--version-script,${LLVM_LIBRARY_DIR}/tools/llvm-shlib/simple_version_script.map ${LIB_NAMES})
        endif()
        if (LLVM_LINKER_SUPPORTS_B_SYMBOLIC_FUNCTIONS)
          # Optimize function calls for default visibility definitions to avoid PLT and
          # reduce dynamic relocations.
          # Note: for -fno-pic default, the address of a function may be different from
          # inside and outside libLLVM.so.
          target_link_options(LLVM PRIVATE LINKER:-Bsymbolic-functions)
        endif()
      endif()
    endif()

    target_link_libraries(LLVM PRIVATE ${LIB_NAMES})
  endif()

  if(LLVM_ENABLE_THREADS AND NOT HAVE_CXX_ATOMICS64_WITHOUT_LIB)
    target_link_libraries(LLVM PUBLIC atomic)
  endif()

  if (APPLE)
    set_property(TARGET LLVM APPEND_STRING PROPERTY
                LINK_FLAGS
                " -compatibility_version 1 -current_version ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}.${LLVM_VERSION_PATCH}")
  endif()

  if(TARGET libLLVMExports)
    add_dependencies(LLVM libLLVMExports)
  endif()
endif()

if(LLVM_BUILD_LLVM_C_DYLIB AND NOT MSVC)
  if(NOT APPLE)
    message(FATAL_ERROR "Generating libLLVM-c is only supported on Darwin")
  endif()

  if(NOT LLVM_BUILD_LLVM_DYLIB)
    message(FATAL_ERROR "Generating libLLVM-c requires LLVM_BUILD_LLVM_C_DYLIB on Darwin")
  endif()

  # To get the export list for a single llvm library:
  # nm ${LIB_PATH} | awk "/T _LLVM/ { print $3 }" | sort -u | sed -e "s/^_//g" > ${LIB_PATH}.exports

  set(LLVM_EXPORTED_SYMBOL_FILE ${LLVM_BINARY_DIR}/libllvm-c.exports)

  set(LIB_DIR ${LLVM_LIBRARY_DIR})
  set(LIB_NAME ${LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}LLVM)
  set(LIB_PATH ${LIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
  set(LIB_EXPORTS_PATH ${LIB_NAME}.exports)
  list(APPEND LLVM_DYLIB_REQUIRED_EXPORTS ${LIB_EXPORTS_PATH})

  add_custom_command(OUTPUT ${LLVM_EXPORTED_SYMBOL_FILE}
    COMMAND nm ${LIB_PATH} | awk "/T _LLVM/ || /T LLVM/ { print $3 }" | sort -u | sed -e "s/^_//g" > ${LLVM_EXPORTED_SYMBOL_FILE}
    WORKING_DIRECTORY ${LIB_DIR}
    DEPENDS LLVM
    COMMENT "Generating Export list for LLVM..."
    VERBATIM )

  add_custom_target(libLLVMCExports DEPENDS ${LLVM_EXPORTED_SYMBOL_FILE})

  add_llvm_library(LLVM-C SHARED ${SOURCES} INSTALL_WITH_TOOLCHAIN)

  target_link_libraries(LLVM-C PUBLIC LLVM)
  add_dependencies(LLVM-C libLLVMCExports)

  set_property(TARGET LLVM-C APPEND_STRING PROPERTY
              LINK_FLAGS
              " -compatibility_version 1 -current_version ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}.${LLVM_VERSION_PATCH} -Wl,-reexport_library ${LIB_PATH}")
endif()

if(LLVM_BUILD_LLVM_C_DYLIB AND MSVC)
  # Build the LLVM-C.dll library that exports the C API.

  set(LLVM_LINK_COMPONENTS
    ${LLVM_DYLIB_COMPONENTS}
    )

  llvm_map_components_to_libnames(LIB_NAMES ${LLVM_DYLIB_COMPONENTS})
  list(REMOVE_DUPLICATES LIB_NAMES)

  # The python script needs to know whether symbols are prefixed with underscores or not.
  if(LLVM_HOST_TRIPLE MATCHES "i?86-.*win.*")
    set(GEN_UNDERSCORE "--underscore")
  else()
    set(GEN_UNDERSCORE "")
  endif()

  # Set this name here, not used in multi conf loop,
  # but add script will pick the right one.
  set(LIBSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/libllvm-c.args)

  # Get the full name to the libs so the python script understands them.
  foreach(lib ${LIB_NAMES})
    list(APPEND FULL_LIB_NAMES ${LLVM_LIBRARY_DIR}/${lib}.lib)
  endforeach()

  # Need to separate lib names with newlines.
  string(REPLACE ";" "\n" FILE_CONTENT "${FULL_LIB_NAMES}")

  if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
    # Write out the full lib names into file to be read by the python script.
    file(WRITE ${LIBSFILE} "${FILE_CONTENT}")
  else()
    foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
      # Replace the special string with a per config directory.
      string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${FILE_CONTENT}")

      # Write out the full lib names into file to be read by the python script.
      # One libsfile per build, the add_custom_command should expand
      # ${CMAKE_CFG_INTDIR} correctly and select the right one.
      file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/libllvm-c.args "${PER_CONF_CONTENT}")
    endforeach()
  endif()

  # Generate the exports file dynamically.
  set(GEN_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/gen-msvc-exports.py)

  set(LLVM_EXPORTED_SYMBOL_FILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/libllvm-c.exports)
  if(LLVM_USE_MSVC_DLLIFY)
    # Avoid a build-cycle: llvm-nm itself links against LLVM.dll when
    # LLVM_LINK_LLVM_DYLIB is enabled, while LLVM.dll needs an nm-compatible
    # tool to inventory the static archives. Use the compiler toolchain's nm.
    set(llvm_nm_exe "${CMAKE_NM}")
    set(llvm_nm_target)
  else()
    get_host_tool_path(llvm-nm LLVM_NM llvm_nm_exe llvm_nm_target)
  endif()

  add_custom_command(OUTPUT ${LLVM_EXPORTED_SYMBOL_FILE}
    COMMAND "${Python3_EXECUTABLE}" ${GEN_SCRIPT} --libsfile ${LIBSFILE} ${GEN_UNDERSCORE} --nm "${llvm_nm_exe}" -o ${LLVM_EXPORTED_SYMBOL_FILE}
    DEPENDS ${LIB_NAMES} ${llvm_nm_target}
    COMMENT "Generating export list for LLVM-C"
    VERBATIM )

  # Finally link the target.
  add_llvm_library(LLVM-C SHARED INSTALL_WITH_TOOLCHAIN ${SOURCES} DEPENDS intrinsics_gen)

  if (LLVM_INTEGRATED_CRT_ALLOC AND MSVC)
    # Make sure we search LLVMSupport first, before the CRT libs
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_CXX_LINKER_WRAPPER_FLAG}-INCLUDE:malloc")
  endif()
  
endif()
