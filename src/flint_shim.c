#include "raylib.h"
#include "flint_ui.h"

// Missing implementations from vendor/flint - provided locally

int ui_scrollbar_content_width(int content_width, int max_scroll) {
    (void)content_width;
    (void)max_scroll;
    return 0;
}

int ui_dropdown_captures_click(Vector2 point) {
    (void)point;
    return 0;
}
