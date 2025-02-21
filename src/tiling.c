#include <inttypes.h>

#include "configuration.h"
#include "log.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"

/* Split a frame horizontally or vertically. */
void split_frame(Frame *split_from, frame_split_direction_t direction)
{
    Frame *left, *right;
    Frame *next_focus_frame;

    split_from->split_direction = direction;

    left = xcalloc(1, sizeof(*left));
    right = xcalloc(1, sizeof(*right));

    switch (direction) {
    /* left to right split */
    case FRAME_SPLIT_HORIZONTALLY:
        left->x = split_from->x;
        left->width = split_from->width / 2;
        right->x = left->x + left->width;
        right->width = split_from->width -
            left->width;

        left->y = split_from->y;
        left->height = split_from->height;
        right->y = split_from->y;
        right->height = split_from->height;
        break;

    /* top to bottom split */
    case FRAME_SPLIT_VERTICALLY:
        left->y = split_from->y;
        left->height = split_from->height / 2;
        right->y = left->y + left->height;
        right->height = split_from->height -
            left->height;

        left->x = split_from->x;
        left->width = split_from->width;
        right->x = split_from->x;
        right->width = split_from->width;
        break;
    }

    left->window = split_from->window;

    split_from->window = NULL;
    split_from->left = left;
    split_from->right = right;

    left->parent = split_from;
    right->parent = split_from;

    if (split_from == focus_frame) {
        next_focus_frame = left;
    } else {
        next_focus_frame = focus_frame;
    }

    if (configuration.tiling.auto_fill_void && last_taken_window != NULL) {
        right->window = last_taken_window;
        show_window(last_taken_window);
        last_taken_window = last_taken_window->previous_taken;
    }

    reload_frame(left);

    set_focus_frame(next_focus_frame);

    LOG("split %p[%p, %p]\n", (void*) split_from, (void*) left, (void*) right);
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

/* Get the frame on the left of @frame. */
Frame *get_right_frame(Frame *frame) {
    return get_right_or_below_frame(frame, FRAME_SPLIT_VERTICALLY);
}

/* Get the frame above @frame. */
Frame *get_below_frame(Frame *frame)
{
    return get_right_or_below_frame(frame, FRAME_SPLIT_HORIZONTALLY);
}

/* Get the minimum size the given frame should have. */
static void get_minimum_frame_size(Frame *frame, Size *size)
{
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
}

/* Increases the @edge of @frame by @amount. */
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

/* Destroys a frame and all sub frames while also unmapping the associated
 * windows if any are there.
 */
static void unmap_and_destroy_recursively(Frame *frame)
{
    if (frame->left != NULL) {
        unmap_and_destroy_recursively(frame->left);
        unmap_and_destroy_recursively(frame->right);
    } else if (frame->window != NULL) {
        hide_window_abruptly(frame->window);
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
        return ERROR;
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
    return OK;
}

/* Destroy a frame and all child frames and hide all inner windows. */
void abandon_frame(Frame *frame)
{
    unmap_and_destroy_recursively(frame);
}
