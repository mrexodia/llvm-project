add_llvm_utility(llvm-test-mustache-spec
  llvm-test-mustache-spec.cpp
)

if(LLVM_USE_MSVC_DLLIFY AND LLVM_LINK_LLVM_DYLIB)
  target_link_libraries(llvm-test-mustache-spec PRIVATE LLVM)
else()
  target_link_libraries(llvm-test-mustache-spec PRIVATE LLVMSupport)
endif()
