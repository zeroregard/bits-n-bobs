#!/bin/sh
# run a command inside the KDE SDK flatpak with llvm20 on PATH
exec flatpak run --command=sh --filesystem=home --share=network org.kde.Sdk//5.15-25.08 -c "export PATH=/usr/lib/sdk/llvm20/bin:\$PATH; $*"
