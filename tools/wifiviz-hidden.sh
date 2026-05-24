#!/usr/bin/env sh

APP="${1:-build/WiFiVizApp}"
shift 2>/dev/null || true

if [ ! -x "$APP" ] && [ -x "WiFiVizApp" ]; then
    APP="WiFiVizApp"
fi

nohup "$APP" "$@" >/dev/null 2>&1 &
