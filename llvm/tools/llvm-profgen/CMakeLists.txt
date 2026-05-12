
set(LLVM_LINK_COMPONENTS
  AllTargetsDescs
  AllTargetsDisassemblers
  AllTargetsInfos
  DebugInfoDWARF
  Core
  MC
  IPO
  MCDisassembler
  Object
  ProfileData
  Support
  Symbolize
  TargetParser
  )

add_llvm_tool(llvm-profgen
  llvm-profgen.cpp
  PerfReader.cpp
  CSPreInliner.cpp
  ProfiledBinary.cpp
  ProfileGenerator.cpp
  MissingFrameInferrer.cpp
  )

if(LLVM_USE_MSVC_DLLIFY)
  # With the resolver-based LLVM.dll stubs, MSVC's normal atexit registration
  # path has been observed to crash before main() in llvm-profgen.  The tool does
  # not depend on process-exit destructors for correctness, so provide local
  # no-op definitions and allow them to override the CRT copies.
  set(noop_atexit ${CMAKE_CURRENT_BINARY_DIR}/noop_atexit.c)
  file(WRITE ${noop_atexit}
    "typedef void (__cdecl *atexit_func_t)(void);\n"
    "int __cdecl atexit(atexit_func_t f) { (void)f; return 0; }\n"
    "int __cdecl _crt_atexit(atexit_func_t f) { (void)f; return 0; }\n")
  target_sources(llvm-profgen PRIVATE ${noop_atexit})
  target_link_options(llvm-profgen PRIVATE /force:multiple)
endif()
