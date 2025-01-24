#include <inttypes.h>
#include <stdint.h>

#include <unistd.h>
#include <signal.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "util.h"

/* TODO: For advanced tiling.
 * 1. Multi monitor support.
 * 2. Frame resizing.
 */

/* Split a frame horizontally or vertically.
 *
 * This cuts she @split_from frame in half and places the next window
 * inside the formed gap. If there is no next window, a gap simply remains.
 */
void split_frame(Frame split_from, int is_split_vert)
{
    Frame   left, right;
    Frame   next_cur_frame;
    Window  *window;

    left = LEFT_FRAME(split_from);
    right = RIGHT_FRAME(split_from);

    if (right >= g_frame_capacity) {
        RESIZE(g_frames, right + 1);
        for (Frame f = g_frame_capacity; f < left; f++) {
            g_frames[f].window = WINDOW_SENTINEL;
        }
        g_frame_capacity = right + 1;
    }

    if (is_split_vert) {
        g_frames[left].y = g_frames[split_from].y;
        g_frames[left].height = g_frames[split_from].height / 2;
        g_frames[right].y = g_frames[left].y + g_frames[left].height;
        g_frames[right].height = g_frames[split_from].height -
            g_frames[left].height;

        g_frames[left].x = g_frames[split_from].x;
        g_frames[left].width = g_frames[split_from].width;
        g_frames[right].x = g_frames[split_from].x;
        g_frames[right].width = g_frames[split_from].width;
    } else {
        g_frames[left].x = g_frames[split_from].x;
        g_frames[left].width = g_frames[split_from].width / 2;
        g_frames[right].x = g_frames[left].x + g_frames[left].width;
        g_frames[right].width = g_frames[split_from].width -
            g_frames[left].width;

        g_frames[left].y = g_frames[split_from].y;
        g_frames[left].height = g_frames[split_from].height;
        g_frames[right].y = g_frames[split_from].y;
        g_frames[right].height = g_frames[split_from].height;
    }

    g_frames[left].window = g_frames[split_from].window;
    g_frames[right].window = NULL;
    g_frames[split_from].window = NULL;
    if (split_from == g_cur_frame) {
        next_cur_frame = left;
    } else {
        next_cur_frame = g_cur_frame;
    }

    g_cur_frame = right;
    window = get_next_hidden_window(g_frames[left].window);
    if (window != NULL) {
        set_window_state(window, WINDOW_STATE_SHOWN, 1);
    }

    g_cur_frame = next_cur_frame;

    if (g_frames[left].window != NULL) {
        reload_frame(left);
    }

    set_notification(UTF8_TEXT("Current frame"),
            g_frames[g_cur_frame].x + g_frames[g_cur_frame].width / 2,
            g_frames[g_cur_frame].y + g_frames[g_cur_frame].height / 2);

    LOG("split %" PRId32 "[%" PRId32 ", %" PRId32 "]\n",
            split_from, left, right);
}

/* Set the size of a frame, this also resize the inner windows.
 *
 * This first checks if the sub frames are in a left and right 
 * orientation (horizontal split) or up and down orientation
 * (vertical split) and then resizes the children appropiately.
 *
 * TODO: keep ratio when resizing?
 */
void resize_frame(Frame frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Frame left, right;

    g_frames[frame].x = x;
    g_frames[frame].y = y;
    g_frames[frame].width = width;
    g_frames[frame].height = height;
    reload_frame(frame);

    left = LEFT_FRAME(frame);
    right = RIGHT_FRAME(frame);

    if (!IS_FRAME_VALID(left)) {
        return;
    }

    if (g_frames[left].x != g_frames[right].x) {
        resize_frame(left, x, y, width / 2, height);
        resize_frame(right, x + g_frames[left].width, y,
                width - g_frames[left].width, height);
    } else {
        resize_frame(left, x, y, width, height / 2);
        resize_frame(right, x, y + g_frames[left].height, width,
                height - g_frames[left].height);
    }
}

/* Shifting frames up is a system to get all nodes up one layer in the tree.
 * For each node, it decrements its children by twice the amount it got
 * decremented themself.
 *
 * Using the shifts assures that frames that are in the upper layer are set
 * first, this is done in `remove_leaf_frame()`.
 */
static void shift_frames_up(Frame first, uint32_t decrement, uint32_t *shifts)
{
    shifts[first] = decrement;
    if (IS_FRAME_VALID(RIGHT_FRAME(first))) {
        decrement *= 2;
        shift_frames_up(LEFT_FRAME(first), decrement, shifts);
        shift_frames_up(RIGHT_FRAME(first), decrement, shifts);
    }
}

/* Remove a frame from the screen and hide the inner window. */
int remove_leaf_frame(Frame frame)
{
    Frame       parent, left, right;
    int32_t     center_x, center_y;
    int32_t     parent_x, parent_y, parent_width, parent_height;
    Window      *hide_window;
    uint32_t    *shifts;

    if (frame == 0) {
        LOG("attempted to remove the root frame\n");
        return 1;
    }

    parent = PARENT_FRAME(frame);

    /* preserve parent size */
    parent_x = g_frames[parent].x;
    parent_y = g_frames[parent].y;
    parent_width = g_frames[parent].width;
    parent_height = g_frames[parent].height;

    center_x = parent_x + parent_width / 2;
    center_y = parent_y + parent_height / 2;

    hide_window = g_frames[frame].window;
    g_frames[frame].window = WINDOW_SENTINEL;

    left = LEFT_FRAME(parent);
    right = RIGHT_FRAME(parent);

    shifts = xcalloc(g_frame_capacity, sizeof(*shifts));
    if (left == frame) {
        shift_frames_up(right, right - parent, shifts);
    } else if (right == frame) {
        shift_frames_up(left, left - parent, shifts);
    }
    for (uint32_t i = 0; i < g_frame_capacity; i++) {
        if (shifts[i] > 0) {
            g_frames[i - shifts[i]] = g_frames[i];
            g_frames[i].window = WINDOW_SENTINEL;
        }
    }
    free(shifts);

    resize_frame(parent, parent_x, parent_y, parent_width, parent_height);

    LOG("frame %" PRId32 " was removed\n", frame);

    if (hide_window != NULL) {
        hide_window->state = WINDOW_STATE_HIDDEN;
        hide_window->focused = 0;
        xcb_unmap_window(g_dpy, hide_window->xcb_window);
    }

    set_focus_frame(get_frame_at_position(center_x, center_y));
    return 0;
}
