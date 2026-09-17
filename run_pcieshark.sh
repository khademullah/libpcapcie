#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP="$SCRIPT_DIR/build/gui/pcieshark"

if [ ! -x "$APP" ]; then
    echo "pcieshark was not built yet. Run: cmake --build build --target pcieshark_gui" >&2
    exit 1
fi

# Force the system pthread/libc stack so Qt can launch correctly from a snap-hosted terminal.
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-xcb}
export LD_LIBRARY_PATH="/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export LD_PRELOAD="/lib/x86_64-linux-gnu/libpthread.so.0${LD_PRELOAD:+:${LD_PRELOAD}}"

exec "$APP" "$@"
