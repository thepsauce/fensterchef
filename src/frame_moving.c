#include "configuration/configuration.h"
#include "frame.h"
#include "frame_splitting.h"
#include "monitor.h"

/* Get the frame on the left of @frame. */
static Frame *get_left_or_above_frame(Frame *frame,
        frame_split_direction_t direction)
{
    Frame *parent;

    while (frame != NULL) {
        parent = frame->parent;
        if (parent == NULL) {
            frame = NULL;
            break;
        }

        if (parent->split_direction != direction) {
            frame = parent;
        } else if (parent->left == frame) {
            frame = parent;
        } else {
            frame = parent->left;
            while (frame->left != NULL &&
                    frame->split_direction == direction) {
                frame = frame->right;
            }
            break;
        }
    }
    return frame;
}

/* Get the frame on the left of @frame. */
Frame *get_left_frame(Frame *frame)
{
    return get_left_or_above_frame(frame, FRAME_SPLIT_HORIZONTALLY);
}

/* Get the frame above @frame. */
Frame *get_above_frame(Frame *frame)
{
    return get_left_or_above_frame(frame, FRAME_SPLIT_VERTICALLY);
}

/* Get the frame on the left of @frame. */
static Frame *get_right_or_below_frame(Frame *frame,
        frame_split_direction_t direction)
{
    Frame *parent;

    while (frame != NULL) {
        parent = frame->parent;
        if (parent == NULL) {
            frame = NULL;
            break;
        }

        if (parent->split_direction != direction) {
            frame = parent;
        } else if (parent->right == frame) {
            frame = parent;
        } else {
            frame = parent->right;
            while (frame->left != NULL &&
                    frame->split_direction == direction) {
                frame = frame->left;
            }
            break;
        }
    }
    return frame;
}

/* Get the frame on the right of @frame. */
Frame *get_right_frame(Frame *frame)
{
    return get_right_or_below_frame(frame, FRAME_SPLIT_HORIZONTALLY);
}

/* Get the frame below @frame. */
Frame *get_below_frame(Frame *frame)
{
    return get_right_or_below_frame(frame, FRAME_SPLIT_VERTICALLY);
}

/* Get the most left within @frame. */
Frame *get_most_left_leaf_frame(Frame *frame, int32_t y)
{
    while (frame->left != NULL) {
        if (frame->split_direction == FRAME_SPLIT_VERTICALLY) {
            if (frame->left->y + (int32_t) frame->left->height >= y) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        } else {
            frame = frame->left;
        }
    }
    return frame;
}

/* Get the frame at the top within @frame. */
Frame *get_top_leaf_frame(Frame *frame, int32_t x)
{
    while (frame->left != NULL) {
        if (frame->split_direction == FRAME_SPLIT_HORIZONTALLY) {
            if (frame->left->x + (int32_t) frame->left->width >= x) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        } else {
            frame = frame->left;
        }
    }
    return frame;
}

/* Get the most right frame within @frame. */
Frame *get_most_right_leaf_frame(Frame *frame, int32_t y)
{
    while (frame->left != NULL) {
        if (frame->split_direction == FRAME_SPLIT_VERTICALLY) {
            if (frame->left->y + (int32_t) frame->left->height >= y) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        } else {
            frame = frame->right;
        }
    }
    return frame;
}

/* Get the frame at the bottom within @frame. */
Frame *get_bottom_leaf_frame(Frame *frame, int32_t x)
{
    while (frame->left != NULL) {
        if (frame->split_direction == FRAME_SPLIT_HORIZONTALLY) {
            if (frame->left->x + (int32_t) frame->left->width >= x) {
                frame = frame->left;
            } else {
                frame = frame->right;
            }
        } else {
            frame = frame->right;
        }
    }
    return frame;
}

/* Utility function for moving frames. */
static void do_resplit(Frame *frame, Frame *original, bool is_left_split,
        frame_split_direction_t direction)
{
    /* if they have the same parent, then `remove_frame()` would
     * invalidate the `frame` pointer, we would need to split off
     * the parent
     */
    if (frame->parent != NULL && frame->parent == original->parent) {
        frame = frame->parent;
    }

    if (is_frame_void(frame)) {
        /* case S1 */
        if (Frame_focus == original) {
            Frame_focus = frame;
        }
        replace_frame(frame, original);
        /* do not destroy root frames */
        if (original->parent != NULL) {
            remove_frame(original);
            destroy_frame(original);
        }
    } else {
        const bool refocus = Frame_focus == original;
        if (original->parent == NULL) {
            /* make a wrapper around the root */
            Frame *const new = create_frame();
            replace_frame(new, original);
            original = new;
        } else {
            /* disconect the frame from the layout */
            remove_frame(original);
        }
        split_frame(frame, original, is_left_split, direction);
        if (refocus) {
            Frame_focus = is_left_split ? frame->left : frame->right;
        }
    }
}

/* Move @frame up or to the left.
 *
 * @direction determines if moving up or to the left.
 *
 * The comments are written for left movement but are analogous to up movement.
 * The cases refer to what is described in the include file.
 */
static bool move_frame_up_or_left(Frame *frame,
        frame_split_direction_t direction)
{
    Frame *original;
    bool is_left_split = false;

    original = frame;

    /* get the parent frame as long as we are on the left of a horizontal split
     */
    while (frame->parent != NULL &&
            frame->parent->split_direction == direction &&
            frame->parent->left == frame) {
        frame = frame->parent;
    }

    /* if we are in a parent that is split vertically, move it left of this
     * parent, unwinding from the vertical split
     */
    if (frame->parent != NULL && frame->parent->split_direction != direction) {
        /* case 3 */
        frame = frame->parent;
        is_left_split = true;
    } else {
        if (direction == FRAME_SPLIT_HORIZONTALLY) {
            frame = get_left_frame(frame);
        } else {
            frame = get_above_frame(frame);
        }

        if (frame == NULL) {
            /* case 1, S2 */
            Monitor *monitor;

            monitor = get_monitor_containing_frame(original);
            if (direction == FRAME_SPLIT_HORIZONTALLY) {
                monitor = get_left_monitor(monitor);
            } else {
                monitor = get_above_monitor(monitor);
            }
            if (monitor == NULL) {
                frame = NULL;
            } else {
                frame = monitor->frame;
            }
        } else {
            if (frame->left != NULL) {
                /* case 2, 5 */
                if (direction == FRAME_SPLIT_HORIZONTALLY) {
                    frame = get_most_right_leaf_frame(frame,
                            original->y + original->height / 2);
                } else {
                    frame = get_bottom_leaf_frame(frame,
                            original->x + original->width / 2);
                }
            } else {
                /* case 4 */
                is_left_split = true;
            }
        }
    }

    if (frame == NULL) {
        /* there is nowhere to move the frame to */
        return false;
    }

    do_resplit(frame, original, is_left_split, direction);
    return true;
}

/* Move @frame to the left. */
bool move_frame_left(Frame *frame)
{
    return move_frame_up_or_left(frame, FRAME_SPLIT_HORIZONTALLY);
}

/* Move @frame up. */
bool move_frame_up(Frame *frame)
{
    return move_frame_up_or_left(frame, FRAME_SPLIT_VERTICALLY);
}

/* Move @frame down or to the right.
 *
 * @direction determines if moving down or to the left.
 *
 * The comments are written for right movement but are analogous to down
 * movement. The cases refer to what is described in the include file.
 */
static bool move_frame_down_or_right(Frame *frame,
        frame_split_direction_t direction)
{
    Frame *original;
    bool is_left_split = true;

    original = frame;

    /* get the parent frame as long as we are on the right of a horizontal split
     */
    while (frame->parent != NULL &&
            frame->parent->split_direction == direction &&
            frame->parent->right == frame) {
        frame = frame->parent;
    }

    /* if we are in a parent that is split vertically, move it right of this
     * parent, unwinding from the vertical split
     */
    if (frame->parent != NULL && frame->parent->split_direction != direction) {
        /* case 3 */
        frame = frame->parent;
        is_left_split = false;
    } else {
        if (direction == FRAME_SPLIT_HORIZONTALLY) {
            frame = get_right_frame(frame);
        } else {
            frame = get_below_frame(frame);
        }

        if (frame == NULL) {
            /* case 1, S2 */
            Monitor *monitor;

            monitor = get_monitor_containing_frame(original);
            if (direction == FRAME_SPLIT_HORIZONTALLY) {
                monitor = get_right_monitor(monitor);
            } else {
                monitor = get_below_monitor(monitor);
            }
            if (monitor == NULL) {
                frame = NULL;
            } else {
                frame = monitor->frame;
            }
        } else {
            if (frame->left != NULL) {
                /* case 2, 5 */
                if (direction == FRAME_SPLIT_HORIZONTALLY) {
                    frame = get_most_right_leaf_frame(frame,
                            original->y + original->height / 2);
                } else {
                    frame = get_bottom_leaf_frame(frame,
                            original->x + original->width / 2);
                }
            } else {
                /* case 4 */
                is_left_split = false;
            }
        }
    }

    if (frame == NULL) {
        /* there is nowhere to move the frame to */
        return false;
    }

    do_resplit(frame, original, is_left_split, direction);
    return true;
}

/* Move @frame to the right. */
bool move_frame_right(Frame *frame)
{
    return move_frame_down_or_right(frame, FRAME_SPLIT_HORIZONTALLY);
}

/* Move @frame down. */
bool move_frame_down(Frame *frame)
{
    return move_frame_down_or_right(frame, FRAME_SPLIT_VERTICALLY);
}

/* Exchange @from with @to.
 *
 * We can assume that both frames are independent.
 */
void exchange_frames(Frame *from, Frame *to)
{
    /* swap the focus if one frame has it */
    if (Frame_focus == from) {
        Frame_focus = to;
    } else if (Frame_focus == to) {
        Frame_focus = from;
    }

    /* if moving into a void, either remove it or replace it and when
     * replacing it, remove a potential new void that could appear
     */
    if (is_frame_void(to)) {
        /* this makes `from` a void */
        replace_frame(to, from);
        /* remove the void created if moving into a root frame (different
         * monitor)
         */
        if (from->parent != NULL) {
            remove_frame(from);
            destroy_frame(from);
        }
    /* swap the two frames `from` and `to` */
    } else {
        Frame saved_frame;

        saved_frame = *from;
        replace_frame(from, to);
        replace_frame(to, &saved_frame);
    }
}
