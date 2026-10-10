set -e
cd ~/src/soes-deps; X=$HOME/opt/xwin
F="--target=x86_64-pc-windows-msvc /nologo /O2 /MD /EHsc -Wno-everything /imsvc $X/crt/include /imsvc $X/sdk/include/ucrt /imsvc $X/sdk/include/um /imsvc $X/sdk/include/shared /imsvc $X/sdk/include/winrt"
mkdir -p obj
clang-cl $F /std:c++20 /I DirectXTK-oct2024/Inc /I DirectXTK-oct2024/Src /c DirectXTK-oct2024/Src/SimpleMath.cpp /Foobj/SimpleMath.obj
clang-cl $F /c inih-r58/ini.c /Foobj/ini.obj
clang-cl $F /std:c++20 /I inih-r58 /c inih-r58/cpp/INIReader.cpp /Foobj/INIReader.obj
llvm-lib /nologo /out:prefix/lib/SimpleMath.lib obj/SimpleMath.obj
llvm-lib /nologo /out:prefix/lib/inih.lib obj/ini.obj
llvm-lib /nologo /out:prefix/lib/INIReader.lib obj/INIReader.obj
echo SMALL-OK
