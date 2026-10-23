#!/usr/bin/env bash
# Build if needed, then run. Extra arguments are forwarded to linyrics.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -x "$DIR/build/linyrics" ]; then
    make -C "$DIR" -s
fi

exec "$DIR/build/linyrics" "$@"
