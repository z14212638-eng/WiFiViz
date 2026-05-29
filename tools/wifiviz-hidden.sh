#!/usr/bin/env sh

APP="${1:-build/WiFiVizApp}"
shift 2>/dev/null || true

if [ ! -x "$APP" ] && [ -x "WiFiVizApp" ]; then
    APP="WiFiVizApp"
fi

if [ -z "$WIFIVIZ_DISABLE_AUTO_SCALE" ] && [ -z "$WIFIVIZ_UI_SCALE" ] && [ -z "$QT_SCALE_FACTOR" ]; then
    SCREEN_SIZE=""

    if command -v xrandr >/dev/null 2>&1; then
        SCREEN_SIZE="$(xrandr --listactivemonitors 2>/dev/null \
            | awk '/\*/ {split($3, a, /[x/+]/); split(a[1], w, "/"); split(a[2], h, "/"); print w[1] " " h[1]; exit}')"

        if [ -z "$SCREEN_SIZE" ]; then
            SCREEN_SIZE="$(xrandr --current 2>/dev/null \
                | awk '/ connected primary/ {for (i = 1; i <= NF; ++i) if ($i ~ /^[0-9]+x[0-9]+[+-]/) {split($i, a, /[x+-]/); print a[1] " " a[2]; exit}}')"
        fi

        if [ -z "$SCREEN_SIZE" ]; then
            SCREEN_SIZE="$(xrandr --current 2>/dev/null \
                | awk '/ connected/ {for (i = 1; i <= NF; ++i) if ($i ~ /^[0-9]+x[0-9]+[+-]/) {split($i, a, /[x+-]/); print a[1] " " a[2]; exit}}')"
        fi
    fi

    if [ -z "$SCREEN_SIZE" ] && command -v xdpyinfo >/dev/null 2>&1; then
        SCREEN_SIZE="$(xdpyinfo 2>/dev/null \
            | awk '/dimensions:/ {split($2, a, "x"); print a[1] " " a[2]; exit}')"
    fi

    if [ -n "$SCREEN_SIZE" ]; then
        SCALE="$(printf '%s\n' "$SCREEN_SIZE" | awk '{
            w = $1;
            h = $2;
            sx = w / 2560.0;
            sy = h / 1440.0;
            scale = sx < sy ? sx : sy;
            if (scale > 0) {
                printf "%.4f", scale;
            }
        }')"

        if [ -n "$SCALE" ]; then
            export QT_ENABLE_HIGHDPI_SCALING=1
            export QT_AUTO_SCREEN_SCALE_FACTOR=0
            export QT_SCALE_FACTOR="$SCALE"
        fi
    fi
fi

nohup "$APP" "$@" >/dev/null 2>&1 &
