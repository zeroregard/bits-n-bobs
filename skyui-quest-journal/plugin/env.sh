X=${XWIN:-$HOME/opt/xwin}
export PATH=/usr/lib/sdk/llvm20/bin:$PATH
CXXFLAGS="--target=x86_64-pc-windows-msvc /nologo /O2 /EHsc /MT /std:c++17 -Wno-everything /imsvc $X/crt/include /imsvc $X/sdk/include/ucrt /imsvc $X/sdk/include/um /imsvc $X/sdk/include/shared"
LDFLAGS="/libpath:$X/crt/lib/x86_64 /libpath:$X/sdk/lib/um/x86_64 /libpath:$X/sdk/lib/ucrt/x86_64"
