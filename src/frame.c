#include <inttypes.h>

#include "configuration/configuration.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "size_frame.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"

/* the last frame in the frame stashed linked list */
Frame *Frame_last_stashed;

/* the currently selected/focused frame */
Frame *Frame_focus;

/* Increment the reference count of the frame. */
inline void reference_frame(Frame *frame)
{
    frame->reference_count++;
}

/* Decrement the reference count of the frame and free @frame when it reaches 0.
 */
inline void dereference_frame(Frame *frame)
{
    frame->reference_count--;
    if (frame->reference_count == 0) {
        free(frame);
    }
}

/* Create a frame object. */
inline Frame *create_frame(void)
{
    Frame *const frame = xcalloc(1, sizeof(*Frame_focus));
    frame->reference_count = 1;
    return frame;
}

/* Free the frame object. */
void destroy_frame(Frame *frame)
{
    Frame *previous;

    if (frame->parent != NULL) {
        LOG_ERROR("the frame being destroyed still has a parent\n");
        remove_frame(frame);
        /* execution should be fine after this point but this still only gets
         * triggered by a bug, frames should always be disconnected before
         * destroying them
         */
    }

    if (frame->left != NULL) {
        LOG_ERROR("the frame being destroyed still has children, "
                "this might leak memory\n");
    }

    /* we could also check for a case where the frame is the root frame of a
     * monitor but checking too much is not good; these things should not happen
     * anyway
     */

    if (frame == Frame_focus) {
        LOG_ERROR("the focused frame is being freed :(\n");
        Frame_focus = NULL;
        /* we can only hope the focused frame is set to something sensible after
         * this, we can not set it to something sensible here
         */
    }

    /* remove from the stash linked list if it is contained in it */
    if (Frame_last_stashed == frame) {
        Frame_last_stashed = Frame_last_stashed->previous_stashed;
    } else {
        previous = Frame_last_stashed;
        while (previous != NULL && previous->previous_stashed != frame) {
            previous = previous->previous_stashed;
        }
        if (previous != NULL) {
            previous->previous_stashed = frame->previous_stashed;
        }
    }

    dereference_frame(frame);
}

/* Check if @frame has given number and if not, try to find a child frame with
 * this number.
 */
static Frame *get_frame_by_number_recursively(Frame *frame, uint32_t number)
{
    Frame *left;

    if (frame->number == number) {
        return frame;
    }

    if (frame->left == NULL) {
        return NULL;
    }

    left = get_frame_by_number_recursively(frame->left, number);
    if (left != NULL) {
        return left;
    }

    return get_frame_by_number_recursively(frame->right, number);
}

/* Look through all visible frames to find a frame with given @number. */
Frame *get_frame_by_number(uint32_t number)
{
    Frame *frame = NULL;

    for (Monitor *monitor = Monitor_first; monitor != NULL;
            monitor = monitor->next) {
        frame = get_frame_by_number_recursively(monitor->frame, number);
        if (frame != NULL) {
            break;
        }
    }
    return frame;
}

/* Check if the given frame has no splits and no window. */
inline bool is_frame_void(const Frame *frame)
{
    /* the frame is a void if it is has both no child frames and no window */
    return frame->left == NULL && frame->window == NULL;
}

/* Check if the given point is within the given frame. */
bool is_point_in_frame(const Frame *frame, int32_t x, int32_t y)
{
    return x >= frame->x && y >= frame->y &&
        x - (int32_t) frame->width < frame->x &&
        y - (int32_t) frame->height < frame->y;
}

/* Get the frame at given position. */
Frame *get_frame_at_position(int32_t x, int32_t y)
{
    Frame *frame;

    for (Monitor *monitor = Monitor_first; monitor != NULL;
            monitor = monitor->next) {
        frame = monitor->frame;
        if (is_point_in_frame(frame, x, y)) {
            /* recursively move into child frame until we are at a leaf */
            while (frame->left != NULL) {
                if (is_point_in_frame(frame->left, x, y)) {
                    frame = frame->left;
                    continue;
                }
                if (is_point_in_frame(frame->right, x, y)) {
                    frame = frame->right;
                    continue;
                }
                return NULL;
            }
            return frame;
        }
    }
    return NULL;
}

/* Replace @frame with @with. */
void replace_frame(Frame *frame, Frame *with)
{
    frame->number = with->number;
    with->number = 0;
    /* reparent the child frames */
    if (with->left != NULL) {
        frame->split_direction = with->split_direction;
        frame->ratio = with->ratio;
        frame->left = with->left;
        frame->right = with->right;
        frame->left->parent = frame;
        frame->right->parent = frame;
        /* one null set for convenience, otherwise the very natural looking
         * swapping in `exchange_frames()` would not work
         */
        frame->window = NULL;

        with->left = NULL;
        with->right = NULL;
    } else {
        frame->window = with->window;
        /* two null sets for convenience */
        frame->left = NULL;
        frame->right = NULL;

        with->window = NULL;
    }

    /* size the children so they fit into their new parent */
    resize_frame_and_ignore_ratio(frame, frame->x, frame->y,
            frame->width, frame->height);
}

/* Get the gaps the frame applies to its inner window. */
void get_frame_gaps(const Frame *frame, Extents *gaps)
{
    Frame *root;

    root = get_root_frame(frame);
    if (root->x == frame->x) {
        gaps->left = configuration.gaps.outer[0];
    } else {
        gaps->left = configuration.gaps.inner[2];
    }

    if (root->y == frame->y) {
        gaps->top = configuration.gaps.outer[1];
    } else {
        gaps->top = configuration.gaps.inner[3];
    }

    if (root->x + root->width == frame->x + frame->width) {
        gaps->right = configuration.gaps.outer[2];
    } else {
        gaps->right = configuration.gaps.inner[0];
    }

    if (root->y + root->height == frame->y + frame->height) {
        gaps->bottom = configuration.gaps.outer[3];
    } else {
        gaps->bottom = configuration.gaps.inner[1];
    }
}

/* Resize the inner window to fit within the frame. */
void reload_frame(Frame *frame)
{
    Extents gaps;

    if (frame->window == NULL) {
        return;
    }

    get_frame_gaps(frame, &gaps);

    gaps.right += gaps.left + configuration.border.size * 2;
    gaps.bottom += gaps.top + configuration.border.size * 2;
    set_window_size(frame->window,
            frame->x + gaps.left,
            frame->y + gaps.top,
            gaps.right > 0 && frame->width < (uint32_t) gaps.right ? 0 :
                frame->width - gaps.right,
            gaps.bottom > 0 && frame->height < (uint32_t) gaps.bottom ? 0 :
                frame->height - gaps.bottom);
}

/* Set the frame in focus, this also focuses the inner window if possible. */
void set_focus_frame(Frame *frame)
{
    set_focus_window(frame->window);

    Frame_focus = frame;
}

/* Focus @window and the frame it is contained in if any. */
void set_focus_window_with_frame(Window *window)
{
    if (window == NULL) {
        set_focus_window(NULL);
    /* if the frame the window is contained in is already focused */
    } else if (Frame_focus->window == window) {
        set_focus_window(window);
    } else {
        Frame *const frame = get_frame_of_window(window);
        if (frame == NULL) {
            set_focus_window(window);
        } else {
            set_focus_frame(frame);
        }
    }
}

/* Get the frame above the given one that has no parent. */
Frame *get_root_frame(const Frame *frame)
{
    if (frame == NULL) {
        return NULL;
    }
    while (frame->parent != NULL) {
        frame = frame->parent;
    }
    return (Frame*) frame;
}
