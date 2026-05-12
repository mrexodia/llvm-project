add_llvm_utility(not
  not.cpp
  )

if(LLVM_USE_MSVC_DLLIFY AND LLVM_LINK_LLVM_DYLIB)
  target_link_libraries(not PRIVATE LLVM)
else()
  target_link_libraries(not PRIVATE LLVMSupport)
endif()
