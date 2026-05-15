# Building libmlir-cpp.so fails if LLVM_ENABLE_PIC=Off.  The MSVC dllify
# implementation is a post-link conversion of static archives and does not need
# PIC.
if (NOT LLVM_ENABLE_PIC AND NOT LLVM_USE_MSVC_DLLIFY)
  return()
endif()

# Building libmlir-cpp.so may not work on MSVC.  The MSVC dllify path below
# produces MLIR.dll from the normal static MLIR archives instead.
if (MSVC AND NOT LLVM_USE_MSVC_DLLIFY)
  return()
endif()

get_property(mlir_libs GLOBAL PROPERTY MLIR_STATIC_LIBS)
get_property(mlir_llvm_link_components GLOBAL PROPERTY MLIR_LLVM_LINK_COMPONENTS)
list(REMOVE_DUPLICATES mlir_libs)
list(REMOVE_DUPLICATES mlir_llvm_link_components)

if(MLIR_LINK_MLIR_DYLIB)
  set(INSTALL_WITH_TOOLCHAIN INSTALL_WITH_TOOLCHAIN)
endif()

if(LLVM_BUILD_LLVM_DYLIB)
  if(LLVM_USE_MSVC_DLLIFY)
    if(NOT CMAKE_C_COMPILER OR CMAKE_C_COMPILER MATCHES "NOTFOUND")
      message(FATAL_ERROR "CMAKE_C_COMPILER is required for the MSVC MLIR.dll generator")
    endif()
    if(NOT CMAKE_LINKER OR CMAKE_LINKER MATCHES "NOTFOUND")
      message(FATAL_ERROR "CMAKE_LINKER is required for the MSVC MLIR.dll generator")
    endif()
    if(NOT CMAKE_AR OR CMAKE_AR MATCHES "NOTFOUND")
      message(FATAL_ERROR "CMAKE_AR is required for the MSVC MLIR.dll generator")
    endif()
    if(NOT CMAKE_NM OR CMAKE_NM MATCHES "NOTFOUND")
      message(FATAL_ERROR "CMAKE_NM is required for the MSVC MLIR.dll generator")
    endif()

    add_mlir_library(
      MLIR
      SHARED
      EXCLUDE_FROM_LIBMLIR
      ${INSTALL_WITH_TOOLCHAIN}
      mlir-shlib.cpp
    )
    add_dependencies(MLIR LLVM ${mlir_libs})

    set(MLIR_MSVC_DLLIFY_STUB_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/dllify-stubs/mlir)
    set(MLIR_MSVC_DLLIFY_STUB_LIBS)
    foreach(lib ${mlir_libs})
      list(APPEND MLIR_MSVC_DLLIFY_STUB_LIBS ${MLIR_MSVC_DLLIFY_STUB_DIR}/${lib}.lib)
    endforeach()
    target_link_libraries(MLIR INTERFACE
      "$<BUILD_INTERFACE:${MLIR_MSVC_DLLIFY_STUB_LIBS}>"
      "$<INSTALL_INTERFACE:${mlir_libs}>")

    set(MLIR_MSVC_DLLIFY_LIBSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/mlir-dllify.args)
    set(MLIR_MSVC_DLLIFY_FULL_LIB_NAMES)
    foreach(lib ${mlir_libs})
      list(APPEND MLIR_MSVC_DLLIFY_FULL_LIB_NAMES ${LLVM_LIBRARY_OUTPUT_INTDIR}/${lib}.lib)
    endforeach()
    string(REPLACE ";" "\n" MLIR_MSVC_DLLIFY_FILE_CONTENT "${MLIR_MSVC_DLLIFY_FULL_LIB_NAMES}")
    if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
      file(WRITE ${MLIR_MSVC_DLLIFY_LIBSFILE} "${MLIR_MSVC_DLLIFY_FILE_CONTENT}")
    else()
      foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
        string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${MLIR_MSVC_DLLIFY_FILE_CONTENT}")
        file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/mlir-dllify.args "${PER_CONF_CONTENT}")
      endforeach()
    endif()

    llvm_map_components_to_libnames(MLIR_MSVC_DLLIFY_LLVM_LIB_NAMES ${LLVM_DYLIB_COMPONENTS})
    list(REMOVE_ITEM MLIR_MSVC_DLLIFY_LLVM_LIB_NAMES LLVMTableGen)
    list(REMOVE_DUPLICATES MLIR_MSVC_DLLIFY_LLVM_LIB_NAMES)
    set(MLIR_MSVC_DLLIFY_LLVM_DEPS)
    foreach(lib ${MLIR_MSVC_DLLIFY_LLVM_LIB_NAMES})
      list(APPEND MLIR_MSVC_DLLIFY_LLVM_DEPS ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/dllify-stubs/${lib}.lib)
    endforeach()
    set(MLIR_MSVC_DLLIFY_LLVM_DEPSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/mlir-llvm-dllify-deps.args)
    string(REPLACE ";" "\n" MLIR_MSVC_DLLIFY_LLVM_DEPS_CONTENT "${MLIR_MSVC_DLLIFY_LLVM_DEPS}")
    if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
      file(WRITE ${MLIR_MSVC_DLLIFY_LLVM_DEPSFILE} "${MLIR_MSVC_DLLIFY_LLVM_DEPS_CONTENT}")
    else()
      foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
        string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${MLIR_MSVC_DLLIFY_LLVM_DEPS_CONTENT}")
        file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/mlir-llvm-dllify-deps.args "${PER_CONF_CONTENT}")
      endforeach()
    endif()

    set(MLIR_MSVC_DLLIFY_WORK_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tools/mlir-shlib/dllify-work)
    add_custom_command(TARGET MLIR POST_BUILD
      COMMAND "${Python3_EXECUTABLE}" "${LLVM_MAIN_SRC_DIR}/tools/llvm-shlib/gen-msvc-dll.py" build
              --libsfile "${MLIR_MSVC_DLLIFY_LIBSFILE}"
              --dll-name MLIR
              --resolver-name __mlir_resolve
              --no-default-c-api-prefix
              --dependency-libsfile "${MLIR_MSVC_DLLIFY_LLVM_DEPSFILE}"
              --dependency-lib "${LLVM_LIBRARY_OUTPUT_INTDIR}/LLVM.lib"
              --output-dir "${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}"
              --bin-dir "${LLVM_RUNTIME_OUTPUT_INTDIR}"
              --out-lib-dir "${MLIR_MSVC_DLLIFY_STUB_DIR}"
              --work-dir "${MLIR_MSVC_DLLIFY_WORK_DIR}"
              --cl "${CMAKE_C_COMPILER}"
              --link "${CMAKE_LINKER}"
              --lib-tool "${CMAKE_AR}"
              --nm "${CMAKE_NM}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MLIR_MSVC_DLLIFY_STUB_DIR}/MLIR.lib" "${LLVM_LIBRARY_OUTPUT_INTDIR}/MLIR.lib"
      BYPRODUCTS "${MLIR_MSVC_DLLIFY_WORK_DIR}/manifest.json" "${MLIR_MSVC_DLLIFY_STUB_DIR}/MLIR.lib" ${MLIR_MSVC_DLLIFY_STUB_LIBS}
      COMMENT "Generating resolver-based MLIR.dll and MLIR component stub libraries"
      VERBATIM)

    if(NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
      install(FILES ${MLIR_MSVC_DLLIFY_STUB_LIBS}
              DESTINATION lib${LLVM_LIBDIR_SUFFIX}
              COMPONENT MLIR
              OPTIONAL)
      install(FILES "${MLIR_MSVC_DLLIFY_WORK_DIR}/manifest.json"
              DESTINATION share
              RENAME mlir-dllify-manifest.json
              COMPONENT MLIR
              OPTIONAL)
    endif()

    if(MLIR_BUILD_MLIR_C_DYLIB)
      message(STATUS "Building MSVC dllify MLIR-C dylib")
      get_property(MLIR_C_MSVC_DLLIFY_LIB_NAMES GLOBAL PROPERTY MLIR_CAPI_LIBS)
      list(REMOVE_DUPLICATES MLIR_C_MSVC_DLLIFY_LIB_NAMES)
      set(MLIR_C_MSVC_DLLIFY_EMBED_LIB_NAMES ${MLIR_C_MSVC_DLLIFY_LIB_NAMES})
      foreach(lib ${MLIR_C_MSVC_DLLIFY_LIB_NAMES})
        get_target_property(capi_link_libs ${lib} MLIR_AGGREGATE_DEP_LIBS_IMPORTED)
        if(NOT capi_link_libs)
          get_target_property(capi_link_libs ${lib} LINK_LIBRARIES)
        endif()
        if(capi_link_libs)
          foreach(dep ${capi_link_libs})
            if(TARGET ${dep} AND dep MATCHES "^MLIR" AND
               NOT dep IN_LIST mlir_libs AND
               NOT dep IN_LIST MLIR_C_MSVC_DLLIFY_LIB_NAMES)
              list(APPEND MLIR_C_MSVC_DLLIFY_EMBED_LIB_NAMES ${dep})
            endif()
          endforeach()
        endif()
      endforeach()
      list(REMOVE_DUPLICATES MLIR_C_MSVC_DLLIFY_EMBED_LIB_NAMES)

      add_mlir_library(
        MLIR-C
        SHARED
        EXCLUDE_FROM_LIBMLIR
        ${INSTALL_WITH_TOOLCHAIN}
        mlir-shlib.cpp
      )
      add_dependencies(MLIR-C MLIR ${MLIR_C_MSVC_DLLIFY_EMBED_LIB_NAMES})

      set(MLIR_C_MSVC_DLLIFY_STUB_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib/dllify-stubs/mlir-c)
      set(MLIR_C_MSVC_DLLIFY_STUB_LIBS)
      foreach(lib ${MLIR_C_MSVC_DLLIFY_EMBED_LIB_NAMES})
        list(APPEND MLIR_C_MSVC_DLLIFY_STUB_LIBS ${MLIR_C_MSVC_DLLIFY_STUB_DIR}/${lib}.lib)
      endforeach()
      target_link_libraries(MLIR-C INTERFACE
        "$<BUILD_INTERFACE:${MLIR_C_MSVC_DLLIFY_STUB_LIBS}>"
        "$<INSTALL_INTERFACE:${MLIR_C_MSVC_DLLIFY_EMBED_LIB_NAMES}>")

      set(MLIR_C_MSVC_DLLIFY_LIBSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/mlir-c-dllify.args)
      set(MLIR_C_MSVC_DLLIFY_FULL_LIB_NAMES)
      foreach(lib ${MLIR_C_MSVC_DLLIFY_EMBED_LIB_NAMES})
        list(APPEND MLIR_C_MSVC_DLLIFY_FULL_LIB_NAMES ${LLVM_LIBRARY_OUTPUT_INTDIR}/${lib}.lib)
      endforeach()
      string(REPLACE ";" "\n" MLIR_C_MSVC_DLLIFY_FILE_CONTENT "${MLIR_C_MSVC_DLLIFY_FULL_LIB_NAMES}")
      if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
        file(WRITE ${MLIR_C_MSVC_DLLIFY_LIBSFILE} "${MLIR_C_MSVC_DLLIFY_FILE_CONTENT}")
      else()
        foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
          string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${MLIR_C_MSVC_DLLIFY_FILE_CONTENT}")
          file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/mlir-c-dllify.args "${PER_CONF_CONTENT}")
        endforeach()
      endif()

      set(MLIR_C_MSVC_DLLIFY_DEPS
        ${LLVM_LIBRARY_OUTPUT_INTDIR}/MLIR.lib
        ${MLIR_MSVC_DLLIFY_STUB_LIBS}
        ${LLVM_LIBRARY_OUTPUT_INTDIR}/LLVM.lib
        ${MLIR_MSVC_DLLIFY_LLVM_DEPS})
      set(MLIR_C_MSVC_DLLIFY_DEPSFILE ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/mlir-c-dllify-deps.args)
      string(REPLACE ";" "\n" MLIR_C_MSVC_DLLIFY_DEPS_CONTENT "${MLIR_C_MSVC_DLLIFY_DEPS}")
      if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
        file(WRITE ${MLIR_C_MSVC_DLLIFY_DEPSFILE} "${MLIR_C_MSVC_DLLIFY_DEPS_CONTENT}")
      else()
        foreach(BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
          string(REPLACE ${CMAKE_CFG_INTDIR} ${BUILD_MODE} PER_CONF_CONTENT "${MLIR_C_MSVC_DLLIFY_DEPS_CONTENT}")
          file(WRITE ${LLVM_BINARY_DIR}/${BUILD_MODE}/mlir-c-dllify-deps.args "${PER_CONF_CONTENT}")
        endforeach()
      endif()

      set(MLIR_C_MSVC_DLLIFY_WORK_DIR ${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}/tools/mlir-shlib/mlir-c-dllify-work)
      add_custom_command(TARGET MLIR-C POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${LLVM_MAIN_SRC_DIR}/tools/llvm-shlib/gen-msvc-dll.py" build
                --libsfile "${MLIR_C_MSVC_DLLIFY_LIBSFILE}"
                --dll-name MLIR-C
                --resolver-name __mlir_c_resolve
                --no-default-c-api-prefix
                --c-api-prefix mlir
                --dependency-libsfile "${MLIR_C_MSVC_DLLIFY_DEPSFILE}"
                --output-dir "${LLVM_BINARY_DIR}/${CMAKE_CFG_INTDIR}"
                --bin-dir "${LLVM_RUNTIME_OUTPUT_INTDIR}"
                --out-lib-dir "${MLIR_C_MSVC_DLLIFY_STUB_DIR}"
                --work-dir "${MLIR_C_MSVC_DLLIFY_WORK_DIR}"
                --cl "${CMAKE_C_COMPILER}"
                --link "${CMAKE_LINKER}"
                --lib-tool "${CMAKE_AR}"
                --nm "${CMAKE_NM}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MLIR_C_MSVC_DLLIFY_STUB_DIR}/MLIR-C.lib" "${LLVM_LIBRARY_OUTPUT_INTDIR}/MLIR-C.lib"
        BYPRODUCTS "${MLIR_C_MSVC_DLLIFY_WORK_DIR}/manifest.json" "${MLIR_C_MSVC_DLLIFY_STUB_DIR}/MLIR-C.lib" ${MLIR_C_MSVC_DLLIFY_STUB_LIBS}
        COMMENT "Generating resolver-based MLIR-C.dll and MLIR C API stub libraries"
        VERBATIM)

      if(NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
        install(FILES ${MLIR_C_MSVC_DLLIFY_STUB_LIBS}
                DESTINATION lib${LLVM_LIBDIR_SUFFIX}
                COMPONENT MLIR-C
                OPTIONAL)
        install(FILES "${MLIR_C_MSVC_DLLIFY_WORK_DIR}/manifest.json"
                DESTINATION share
                RENAME mlir-c-dllify-manifest.json
                COMPONENT MLIR-C
                OPTIONAL)
      endif()
    endif()
  else()
    foreach (lib ${mlir_libs})
      if(XCODE)
        # Xcode doesn't support object libraries, so we have to trick it into
        # linking the static libraries instead.
        list(APPEND _DEPS "-force_load" ${lib})
      else()
        list(APPEND _OBJECTS $<TARGET_OBJECTS:obj.${lib}>)
      endif()
      # libClang needs this, but it causes problems for MLIR (probably
      # because we use public library dependencies within MLIR.)
      # list(APPEND _DEPS  $<TARGET_PROPERTY:${lib},LINK_LIBRARIES>)
    endforeach ()

    add_mlir_library(
      MLIR
      SHARED
      EXCLUDE_FROM_LIBMLIR
      ${INSTALL_WITH_TOOLCHAIN}
      mlir-shlib.cpp
      ${_OBJECTS}
      LINK_LIBS
      ${_DEPS}

      LINK_COMPONENTS
      ${mlir_llvm_link_components}
    )
    target_link_libraries(MLIR PRIVATE ${LLVM_PTHREAD_LIB})
  endif()
endif()

#message("Libraries included in libMLIR.so: ${mlir_libs}")
#message("LLVM Components included in libMLIR.so: ${mlir_llvm_link_components}")

mlir_check_all_link_libraries(MLIR)
