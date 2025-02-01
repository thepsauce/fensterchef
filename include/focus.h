#ifndef FOCUS_H
#define FOCUS_H

#include "screen.h"
#include "frame.h"

struct focus {
    Screen screen;
    Frame *frame;
} g_focused;

/* Set the currently focused screen. */
void set_focus(Screen screen, Frame *frame);

/* Get the screen containing given frame.
 *
 * @return a valid screen index for a valid frame pointer.
 */
Screen get_screen_of_frame(Frame *frame);

#endif
