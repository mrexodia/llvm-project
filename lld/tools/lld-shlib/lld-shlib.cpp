// Empty translation unit used only to give CMake a normal shared-library target.
// The MSVC dllify post-build step replaces the placeholder with lld.dll built
// from the lld static component libraries.
extern "C" void lld_shlib_anchor() {}
