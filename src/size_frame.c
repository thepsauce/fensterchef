#include "move_frame.h"
#include "utility.h"
#include "size_frame.h"

/* Apply the auto equalizationg to given frame. */
void apply_auto_equalize(Frame *to)
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

/* Get the minimum size the given frame should have. */
void get_minimum_frame_size(Frame *frame, Size *size)
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

/* Set the size of a frame, this also resizes the inner frames and windows. */
void resize_frame(Frame *frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Frame *left, *right;
    uint32_t left_size;

    frame->x = x;
    frame->y = y;
    frame->width = width;
    frame->height = height;
    reload_frame(frame);

    left = frame->left;
    right = frame->right;

    /* check if the frame has children */
    if (left == NULL) {
        return;
    }

    switch (frame->split_direction) {
    /* left to right split */
    case FRAME_SPLIT_HORIZONTALLY:
        left_size = (uint64_t) width * frame->ratio.numerator /
            frame->ratio.denominator;
        resize_frame(left, x, y, left_size, height);
        resize_frame(right, x + left_size, y, width - left_size, height);
        break;

    /* top to bottom split */
    case FRAME_SPLIT_VERTICALLY:
        left_size = (uint64_t) height * frame->ratio.numerator /
            frame->ratio.denominator;
        resize_frame(left, x, y, width, left_size);
        resize_frame(right, x, y + left_size, width, height - left_size);
        break;
    }
}

/* Set the size of a frame, this also resizes the child frames and windows. */
void resize_frame_and_ignore_ratio(Frame *frame, int32_t x, int32_t y,
        uint32_t width, uint32_t height)
{
    Frame *left, *right;
    uint32_t left_size;

    frame->x = x;
    frame->y = y;
    frame->width = width;
    frame->height = height;
    reload_frame(frame);

    left = frame->left;
    right = frame->right;

    /* check if the frame has children */
    if (left == NULL) {
        return;
    }

    switch (frame->split_direction) {
    /* left to right split */
    case FRAME_SPLIT_HORIZONTALLY:
        /* keep ratio when resizing or use the default 1/2 ratio */
        left_size = left->width == 0 || right->width == 0 ? width / 2 :
            width * left->width / (left->width + right->width);
        resize_frame_and_ignore_ratio(left, x, y, left_size, height);
        resize_frame_and_ignore_ratio(right, x + left_size, y,
                width - left_size, height);
        break;

    /* top to bottom split */
    case FRAME_SPLIT_VERTICALLY:
        /* keep ratio when resizing or use the default 1/2 ratio */
        left_size = left->height == 0 || right->height == 0 ? height / 2 :
            height * left->height / (left->height + right->height);
        resize_frame_and_ignore_ratio(left, x, y, width, left_size);
        resize_frame_and_ignore_ratio(right, x, y + left_size,
                width, height - left_size);
        break;
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
