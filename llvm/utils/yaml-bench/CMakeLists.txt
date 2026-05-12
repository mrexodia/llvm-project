add_llvm_utility(yaml-bench
  YAMLBench.cpp
  )

if(LLVM_USE_MSVC_DLLIFY AND LLVM_LINK_LLVM_DYLIB)
  target_link_libraries(yaml-bench PRIVATE LLVM)
else()
  target_link_libraries(yaml-bench PRIVATE LLVMSupport)
endif()
