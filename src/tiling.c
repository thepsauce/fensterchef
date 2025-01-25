#include <inttypes.h>

#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "util.h"

/* TODO: For advanced tiling.
 * 1. Multi monitor support.
 * 2. Frame resizing.
 */

/* Grows the frame array so that the frame index fits in the array. */
void make_frame_index_valid(Frame frame)
{
    if (frame >= g_frame_capacity) {
        RESIZE(g_frames, frame + 1);
        for (; g_frame_capacity < frame; g_frame_capacity++) {
            g_frames[g_frame_capacity].window = WINDOW_SENTINEL;
        }
        g_frame_capacity++;
    }
}

/* Split a frame horizontally or vertically.
 *
 * This cuts the @split_from frame in half and places the next window
 * inside the formed gap. If there is no next window, a gap simply remains.
 */
void split_frame(Frame split_from, int is_split_vert)
{
    Frame   left, right;
    Frame   next_cur_frame;
    Window  *window;

    left = LEFT_FRAME(split_from);
    right = RIGHT_FRAME(split_from);

    make_frame_index_valid(right);

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

    window = get_next_hidden_window(g_frames[left].window);
    if (window != NULL) {
        g_cur_frame = right;
        set_window_state(window, WINDOW_STATE_SHOWN, 1);
    }

    if (g_frames[left].window != NULL) {
        reload_frame(left);
    }

    set_focus_frame(next_cur_frame);

    LOG("split %" PRId32 "[%" PRId32 ", %" PRId32 "]\n",
            split_from, left, right);
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
