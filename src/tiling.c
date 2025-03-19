#include <inttypes.h>

#include "configuration.h"
#include "log.h"
#include "stash_frame.h"
#include "tiling.h"
#include "utility.h"
#include "window.h"

/* Apply the auto equalizationg to given frame.
 *
 * This moves up while the split direction matches the split direction of @to.
 */
static void apply_auto_equalize(Frame *to)
{
    frame_split_direction_t direction;
    Frame *start_from;

    direction = to->split_direction;
    start_from = to;
    while (to->parent != NULL) {
        if (to->parent->split_direction == direction) {
            start_from = to->parent;
        }
        to = to->parent;
    }
    equalize_frame(start_from, direction);
}

/* Split a frame horizontally or vertically. */
void split_frame(Frame *split_from, Frame *right,
        frame_split_direction_t direction)
{
    Frame *left;

    left = create_frame();
    if (right == NULL) {
        right = create_frame();
        if (configuration.tiling.auto_fill_void) {
            fill_void_with_stash(right);
        }
    }

    left->number = split_from->number;
    split_from->number = 0;
    /* let `left` take the children or window */
    if (split_from->left != NULL) {
        left->split_direction = split_from->split_direction;
        left->ratio = split_from->ratio;
        left->left = split_from->left;
        left->right = split_from->right;
        left->left->parent = left;
        left->right->parent = left;
    } else {
        left->window = split_from->window;
        split_from->window = NULL;
    }

    split_from->split_direction = direction;
    /* ratio of 1/2 */
    split_from->ratio.numerator = 1;
    split_from->ratio.denominator = 2;
    split_from->left = left;
    split_from->right = right;
    left->parent = split_from;
    right->parent = split_from;

    if (split_from == Frame_focus) {
        Frame_focus = left;
    }

    /* size the child frames */
    resize_frame(split_from, split_from->x, split_from->y,
            split_from->width, split_from->height);
    if (configuration.tiling.auto_equalize) {
        apply_auto_equalize(split_from);
    }

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
Frame *get_left_frame(Frame *frame)
{
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

/* Increase the @edge of @frame by @amount. */
int32_t bump_frame_edge(Frame *frame, frame_edge_t edge, int32_t amount)
{
    Frame *parent, *right;
    Size size;
    int32_t space;

    parent = frame->parent;

    if (parent == NULL || amount == 0) {
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

    /* adjust the ratio */
    if (parent->split_direction == FRAME_SPLIT_HORIZONTALLY) {
        parent->ratio.numerator = parent->left->width;
        parent->ratio.denominator = parent->left->width + parent->right->width;
    } else {
        parent->ratio.numerator = parent->left->height;
        parent->ratio.denominator = parent->left->height + parent->right->height;
    }
    return amount;
}

/* Count the frames in horizontal direction. */
static uint32_t count_horizontal_frames(Frame *frame)
{
    uint32_t left_count, right_count;

    if (frame->left == NULL) {
        return 1;
    }

    left_count = count_horizontal_frames(frame->left);
    right_count = count_horizontal_frames(frame->right);
    switch (frame->split_direction) {
    case FRAME_SPLIT_VERTICALLY:
        return MAX(left_count, right_count);

    case FRAME_SPLIT_HORIZONTALLY:
        return left_count + right_count;
    }

    /* this code is never reached */
    return 0;
}

/* Count the frames in vertical direction. */
static uint32_t count_vertical_frames(Frame *frame)
{
    uint32_t left_count, right_count;

    if (frame->left == NULL) {
        return 1;
    }

    left_count = count_vertical_frames(frame->left);
    right_count = count_vertical_frames(frame->right);
    switch (frame->split_direction) {
    case FRAME_SPLIT_VERTICALLY:
        return left_count + right_count;

    case FRAME_SPLIT_HORIZONTALLY:
        return MAX(left_count, right_count);
    }

    /* this code is never reached */
    return 0;
}

/* Set the size of all children of @frame to be equal within a certain
 * direction.
 */
void equalize_frame(Frame *frame, frame_split_direction_t direction)
{
    uint32_t left_count, right_count;

    /* check if the frame has any children */
    if (frame->left == NULL) {
        return;
    }

    if (direction == frame->split_direction) {
        switch (direction) {
        case FRAME_SPLIT_HORIZONTALLY:
            left_count = count_horizontal_frames(frame->left);
            right_count = count_horizontal_frames(frame->right);
            frame->left->width = frame->width * left_count /
                (left_count + right_count);
            frame->right->x = frame->x + frame->left->width;
            frame->right->width = frame->width - frame->left->width;
            break;

        case FRAME_SPLIT_VERTICALLY:
            left_count = count_vertical_frames(frame->left);
            right_count = count_vertical_frames(frame->right);
            frame->left->height = frame->height * left_count /
                (left_count + right_count);
            frame->right->y = frame->y + frame->left->height;
            frame->right->height = frame->height - frame->left->height;
            break;
        }
    }

    equalize_frame(frame->left, direction);
    equalize_frame(frame->right, direction);
}

/* Remove an empty frame from the screen. */
int remove_void(Frame *frame)
{
    Frame *parent, *other;

    if (frame->parent == NULL) {
        LOG("can not remove the root frame %F\n", frame);
        return ERROR;
    }

    if (!is_frame_void(frame)) {
        LOG_ERROR("frame %F is not a void you foolish developer!\n", frame);
        return ERROR;
    }

    parent = frame->parent;

    if (frame == parent->left) {
        other = parent->right;
    } else {
        other = parent->left;
    }

    parent->number = other->number;
    parent->left = other->left;
    parent->right = other->right;
    if (other->left != NULL) {
        parent->split_direction = other->split_direction;
        parent->ratio = other->ratio;
        parent->left->parent = parent;
        parent->right->parent = parent;
    } else {
        parent->window = other->window;
    }

    resize_frame(parent, parent->x, parent->y, parent->width,
            parent->height);

    LOG("frame %F was removed\n", frame);

    if (Frame_focus->parent == parent) {
        Frame *new;

        const int x = parent->x + parent->width / 2;
        const int y = parent->y + parent->height / 2;
        new = parent;
        /* move down the parent to find the most centered new frame to focus */
        while (new->left != NULL) {
            if (new->split_direction == FRAME_SPLIT_HORIZONTALLY) {
                if (new->left->x + (int32_t) new->left->width >= x) {
                    new = new->left;
                } else {
                    new = new->right;
                }
            } else {
                if (new->left->y + (int32_t) new->left->height >= y) {
                    new = new->left;
                } else {
                    new = new->right;
                }
            }
        }

        Frame_focus = new;
    }

    if (configuration.tiling.auto_equalize) {
        apply_auto_equalize(parent);
    }

    destroy_frame(other);
    destroy_frame(frame);
    return OK;
}
