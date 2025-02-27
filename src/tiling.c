#include <inttypes.h>

#include "configuration.h"
#include "log.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"

/* Split a frame horizontally or vertically. */
void split_frame(Frame *split_from, frame_split_direction_t direction)
{
    Frame *left, *right;
    Frame *next_focus_frame;

    left = xcalloc(1, sizeof(*left));
    right = xcalloc(1, sizeof(*right));

    /* let `left` take the children or window */
    if (split_from->left != NULL) {
        left->split_direction = split_from->split_direction;
        left->left = split_from->left;
        left->right = split_from->right;
        left->left->parent = left;
        left->right->parent = left;
    } else {
        left->window = split_from->window;
        split_from->window = NULL;
    }

    split_from->split_direction = direction;
    split_from->left = left;
    split_from->right = right;
    left->parent = split_from;
    right->parent = split_from;

    if (split_from == focus_frame) {
        next_focus_frame = left;
    } else {
        next_focus_frame = focus_frame;
    }

    /* size the child frames */
    resize_frame(split_from, split_from->x, split_from->y, split_from->width,
            split_from->height);

    if (configuration.tiling.auto_fill_void) {
        fill_void_with_stash(right);
    }

    set_focus_frame(next_focus_frame);

    LOG("split %F(%F, %F)\n", split_from, left, right);
}

/* Get the frame on the left of @frame. */
Frame *get_left_or_above_frame(Frame *frame,
        frame_split_direction_t split_direction)
{
    Frame *parent;

    while (frame != NULL) {
        parent = frame->parent;
        if (parent == NULL) {
            return NULL;
        }

        if (parent->split_direction == split_direction) {
            frame = parent;
        } else if (parent->left == frame) {
            frame = parent;
        } else {
            return parent->left;
        }
    }
    return NULL;
}

/* Get the frame on the left of @frame. */
Frame *get_left_frame(Frame *frame) {
    return get_left_or_above_frame(frame, FRAME_SPLIT_VERTICALLY);
}

/* Get the frame above @frame. */
Frame *get_above_frame(Frame *frame)
{
    return get_left_or_above_frame(frame, FRAME_SPLIT_HORIZONTALLY);
}

/* Get the frame on the left of @frame. */
Frame *get_right_or_below_frame(Frame *frame,
        frame_split_direction_t split_direction)
{
    Frame *parent;

    while (frame != NULL) {
        parent = frame->parent;
        if (parent == NULL) {
            return NULL;
        }

        if (parent->split_direction == split_direction) {
            frame = parent;
        } else if (parent->right == frame) {
            frame = parent;
        } else {
            return parent->right;
        }
    }
    return NULL;
}

/* Get the frame on the right of @frame. */
Frame *get_right_frame(Frame *frame)
{
    return get_right_or_below_frame(frame, FRAME_SPLIT_VERTICALLY);
}

/* Get the frame below @frame. */
Frame *get_below_frame(Frame *frame)
{
    return get_right_or_below_frame(frame, FRAME_SPLIT_HORIZONTALLY);
}

/* Get the minimum size the given frame should have. */
static void get_minimum_frame_size(Frame *frame, Size *size)
{
    Frame *root;

    root = get_root_frame(frame);
    if (frame->left != NULL) {
        Size left_size, right_size;

        get_minimum_frame_size(frame->left, &left_size);
        get_minimum_frame_size(frame->right, &right_size);
        if (frame->split_direction == FRAME_SPLIT_VERTICALLY) {
            size->width = MAX(left_size.width, right_size.width);
            size->height = left_size.height + right_size.height;
        } else {
            size->width = left_size.width + right_size.width;
            size->height = MAX(left_size.height, right_size.height);
        }
    } else {
        size->width = FRAME_MINIMUM_SIZE;
        size->height = FRAME_MINIMUM_SIZE;
    }

    if (frame->x == root->x) {
        size->width += configuration.gaps.outer;
    } else {
        size->width += configuration.gaps.inner;
    }
    if (frame->x + frame->width == root->x + root->width) {
        size->width += configuration.gaps.outer;
    }

    if (frame->y == root->y) {
        size->height += configuration.gaps.outer;
    } else {
        size->height += configuration.gaps.inner;
    }
    if (frame->y + frame->height == root->y + root->height) {
        size->height += configuration.gaps.outer;
    }
}

/* Increase the @edge of @frame by @amount. */
int32_t bump_frame_edge(Frame *frame, frame_edge_t edge, int32_t amount)
{
    Frame *right;
    Size size;
    int32_t space;

    if (amount == 0) {
        return 0;
    }

    switch (edge) {
    /* delegate left movement to right movement */
    case FRAME_EDGE_LEFT:
        frame = get_left_frame(frame);
        if (frame == NULL) {
            return 0;
        }
        amount = -bump_frame_edge(frame, FRAME_EDGE_RIGHT, -amount);
        break;

    /* delegate top movement to bottom movement */
    case FRAME_EDGE_TOP:
        frame = get_above_frame(frame);
        if (frame == NULL) {
            return 0;
        }
        amount = -bump_frame_edge(frame, FRAME_EDGE_BOTTOM, -amount);
        break;

    /* move the frame's right edge */
    case FRAME_EDGE_RIGHT:
        right = get_right_frame(frame);
        if (right == NULL) {
            return 0;
        }
        frame = get_left_frame(right);
        if (amount < 0) {
            get_minimum_frame_size(frame, &size);
            space = size.width - frame->width;
            if (space >= 0) {
                return 0;
            }
            amount = MAX(amount, space);
        } else {
            get_minimum_frame_size(right, &size);
            space = right->width - size.width;
            if (space <= 0) {
                return 0;
            }
            amount = MIN(amount, space);
        }
        resize_frame(frame, frame->x, frame->y, frame->width + amount,
                frame->height);
        resize_frame(right, right->x + amount, right->y, right->width - amount,
                right->height);
        break;

    /* move the frame's bottom edge */
    case FRAME_EDGE_BOTTOM:
        right = get_below_frame(frame);
        if (right == NULL) {
            return 0;
        }
        frame = get_above_frame(right);
        if (amount < 0) {
            get_minimum_frame_size(frame, &size);
            space = size.height - frame->height;
            if (space >= 0) {
                return 0;
            }
            amount = MAX(amount, space);
        } else {
            get_minimum_frame_size(right, &size);
            space = right->height - size.height;
            if (space <= 0) {
                return 0;
            }
            amount = MIN(amount, space);
        }
        resize_frame(frame, frame->x, frame->y, frame->width,
                frame->height + amount);
        resize_frame(right, right->x, right->y + amount, right->width,
                right->height - amount);
        break;
    }
    return amount;
}

/* Remove an empty frame from the screen. */
int remove_void(Frame *frame)
{
    Frame *parent, *other;

    if (frame->parent == NULL) {
        LOG("can not remove the root frame %F\n", frame);
        return ERROR;
    }

    parent = frame->parent;

    if (frame == parent->left) {
        other = parent->right;
    } else {
        other = parent->left;
    }

    parent->left = other->left;
    parent->right = other->right;
    if (other->left != NULL) {
        parent->split_direction = other->split_direction;
        parent->left->parent = parent;
        parent->right->parent = parent;
    } else {
        parent->window = other->window;
    }

    free(other);

    resize_frame(parent, parent->x, parent->y, parent->width, parent->height);

    LOG("frame %F was removed\n", frame);

    free(frame);

    set_focus_frame(parent);
    return OK;
}
