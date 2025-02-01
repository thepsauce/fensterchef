#include <inttypes.h>

#include "fensterchef.h"
#include "log.h"
#include "tiling.h"
#include "util.h"

/* the frames the user abandoned, for example by disconnecting a monitor */
Frame **g_abandoned_frames;
/* the number of frames abandoned */
uint32_t g_abandoned_frame_count;

/* TODO: For advanced tiling.
 * 1. Multi monitor support.
 * 2. Frame resizing.
 */

/* Split a frame horizontally or vertically.
 *
 * This cuts the @split_from frame in half and places the next window
 * inside the formed gap. If there is no next window, a gap simply remains.
 */
void split_frame(Frame *split_from, int is_split_vert)
{
    Frame *left, *right;
    Frame *next_cur_frame;
    Window *window;

    left = xcalloc(1, sizeof(*left));
    right = xcalloc(1, sizeof(*right));

    if (is_split_vert) {
        left->y = split_from->y;
        left->height = split_from->height / 2;
        right->y = left->y + left->height;
        right->height = split_from->height -
            left->height;

        left->x = split_from->x;
        left->width = split_from->width;
        right->x = split_from->x;
        right->width = split_from->width;
    } else {
        left->x = split_from->x;
        left->width = split_from->width / 2;
        right->x = left->x + left->width;
        right->width = split_from->width -
            left->width;

        left->y = split_from->y;
        left->height = split_from->height;
        right->y = split_from->y;
        right->height = split_from->height;
    }

    left->window = split_from->window;

    if (split_from->window != NULL) {
        split_from->window->frame = left;
    }
    split_from->window = NULL;
    split_from->left = left;
    split_from->right = right;

    left->parent = split_from;
    right->parent = split_from;

    if (split_from == g_cur_frame) {
        next_cur_frame = left;
    } else {
        next_cur_frame = g_cur_frame;
    }

    window = get_next_hidden_window(left->window);
    if (window != NULL) {
        g_cur_frame = right;
        set_window_state(window, WINDOW_STATE_SHOWN, 1);
    }

    if (left->window != NULL) {
        set_window_size(left->window, left->x, left->y,
                left->width, left->height);
    }

    set_focus_frame(next_cur_frame);

    LOG("split %p[%p, %p]\n", (void*) split_from, (void*) left, (void*) right);
}

/* Destroys a frame and all sub frames while also unmapping the associated
 * windows if any are there.
 */
static void unmap_and_destroy_recursively(Frame *frame)
{
    if (frame->left != NULL) {
        unmap_and_destroy_recursively(frame->left);
        unmap_and_destroy_recursively(frame->right);
    } else if (frame->window != NULL) {
        frame->window->state.previous = frame->window->state.current;
        frame->window->state.current = WINDOW_STATE_HIDDEN;
        frame->window->focused = 0;
        frame->window->frame = NULL;
        xcb_unmap_window(g_dpy, frame->window->xcb_window);
    }
    free(frame);
}

/* Remove a frame from the screen and hide the inner windows. */
int remove_frame(Frame *frame)
{
    Frame *parent, *other;
    int32_t center_x, center_y;

    if (frame->parent == NULL) {
        LOG("attempted to remove the root frame\n");
        return 1;
    }

    parent = frame->parent;

    if (frame == parent->left) {
        other = parent->right;
    } else {
        other = parent->left;
    }
    parent->window = other->window;
    parent->left = other->left;
    parent->right = other->right;
    free(other);

    if (parent->left != NULL) {
        parent->left->parent = parent;
        parent->right->parent = parent;
    }

    resize_frame(parent, parent->x, parent->y, parent->width, parent->height);

    LOG("frame %p was removed\n", (void*) frame);

    unmap_and_destroy_recursively(frame);

    center_x = parent->x + parent->width / 2;
    center_y = parent->y + parent->height / 2;
    set_focus_frame(get_frame_at_position(center_x, center_y));
    return 0;
}

/* Destroy a frame and all child frames and hide all inner windows.
 */
void abandon_frame(Frame *frame)
{
    RESIZE(g_abandoned_frames, g_abandoned_frame_count + 1);
    g_abandoned_frames[g_abandoned_frame_count++] = frame;
    unmap_and_destroy_recursively(frame);
}
