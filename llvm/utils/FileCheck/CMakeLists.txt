add_llvm_utility(FileCheck
  FileCheck.cpp
  )

if(LLVM_USE_MSVC_DLLIFY AND LLVM_LINK_LLVM_DYLIB)
  target_link_libraries(FileCheck PRIVATE LLVM)
else()
  target_link_libraries(FileCheck PRIVATE LLVMFileCheck LLVMSupport)
endif()
