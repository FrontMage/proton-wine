#!/bin/sh
set -e
if git apply --check "./android/patches/server_protocol.def.patch"; then
    git apply "./android/patches/server_protocol.def.patch"
elif git apply --reverse --check "./android/patches/server_protocol.def.patch"; then
    echo "Android server protocol changes are already present"
else
    echo "Android server protocol changes conflict with the current source" >&2
    exit 1
fi
./autogen.sh
