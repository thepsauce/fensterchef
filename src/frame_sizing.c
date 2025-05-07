#include "frame_moving.h"
#include "frame_sizing.h"
#include "utility/utility.h"
#include "log.h"

/* Apply the auto equalizationg to given frame. */
void apply_auto_equalize(Frame *to, frame_split_direction_t direction)
{
    Frame *start_from;

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
    Extents gaps;

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

    get_frame_gaps(frame, &gaps);
    size->width += gaps.left + gaps.right;
    size->height += gaps.top + gaps.bottom;
}

/* Set the size of a frame, this also resizes the inner frames and windows. */
void resize_frame(Frame *frame, int x, int y,
        unsigned width, unsigned height)
{
    Frame *left, *right;
    unsigned left_size;

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
        left_size = frame->ratio.denominator == 0 ? width / 2 :
                (uint64_t) width * frame->ratio.numerator /
                    frame->ratio.denominator;
        resize_frame(left, x, y, left_size, height);
        resize_frame(right, x + left_size, y, width - left_size, height);
        break;

    /* top to bottom split */
    case FRAME_SPLIT_VERTICALLY:
        left_size = frame->ratio.denominator == 0 ? height / 2 :
                (uint64_t) height * frame->ratio.numerator /
                    frame->ratio.denominator;
        resize_frame(left, x, y, width, left_size);
        resize_frame(right, x, y + left_size, width, height - left_size);
        break;
    }
}

/* Set the size of a frame, this also resizes the child frames and windows. */
void resize_frame_and_ignore_ratio(Frame *frame, int x, int y,
        unsigned width, unsigned height)
{
    Frame *left, *right;
    unsigned left_size;

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
                (uint64_t) width * left->width / (left->width + right->width);
        resize_frame_and_ignore_ratio(left, x, y, left_size, height);
        resize_frame_and_ignore_ratio(right, x + left_size, y,
                width - left_size, height);
        break;

    /* top to bottom split */
    case FRAME_SPLIT_VERTICALLY:
        /* keep ratio when resizing or use the default 1/2 ratio */
        left_size = left->height == 0 || right->height == 0 ? height / 2 :
              (uint64_t) height * left->height / (left->height + right->height);
        resize_frame_and_ignore_ratio(left, x, y, width, left_size);
        resize_frame_and_ignore_ratio(right, x, y + left_size,
                width, height - left_size);
        break;
    }
}

/* Set the ratio, position and size of all parents of @frame to match their
 * children whenever their split direction matches @direction.
 */
static inline void propagate_size(Frame *frame,
        frame_split_direction_t direction)
{
    while (frame->parent != NULL) {
        frame = frame->parent;
        if (frame->split_direction == direction) {
            if (direction == FRAME_SPLIT_HORIZONTALLY) {
                frame->ratio.numerator = frame->right->width;
                frame->ratio.denominator = frame->left->width +
                    frame->right->width;

                frame->x = frame->left->x;
                frame->width = frame->left->width + frame->right->width;
            } else {
                frame->ratio.numerator = frame->right->height;
                frame->ratio.denominator = frame->left->height +
                    frame->right->height;

                frame->y = frame->left->y;
                frame->height = frame->left->height + frame->right->height;
            }
        }
    }
}

/* Increase the @edge of @frame by @amount. */
int bump_frame_edge(Frame *frame, frame_edge_t edge, int amount)
{
    Frame *parent, *right = NULL;
    Size size;
    int space;
    int self_amount;

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
        return -bump_frame_edge(frame, FRAME_EDGE_RIGHT, -amount);

    /* delegate top movement to bottom movement */
    case FRAME_EDGE_TOP:
        frame = get_above_frame(frame);
        if (frame == NULL) {
            return 0;
        }
        return -bump_frame_edge(frame, FRAME_EDGE_BOTTOM, -amount);

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
            space = MIN(space, 0);
            self_amount = MAX(amount, space);
            if (space > amount) {
                /* try to get more space by pushing the left edge */
                space -= bump_frame_edge(frame, FRAME_EDGE_LEFT,
                        -amount + space);
                amount = MAX(amount, space);
            } else {
                amount = self_amount;
            }
        } else {
            get_minimum_frame_size(right, &size);
            space = right->width - size.width;
            space = MAX(space, 0);
            self_amount = MIN(amount, space);
            if (space < amount) {
                /* try to get more space by pushing the right edge */
                space += bump_frame_edge(right, FRAME_EDGE_RIGHT,
                        amount - space);
                amount = MIN(amount, space);
            } else {
                amount = self_amount;
            }
        }

        resize_frame_and_ignore_ratio(frame, frame->x, frame->y,
                frame->width + self_amount, frame->height);
        resize_frame_and_ignore_ratio(right, right->x + self_amount, right->y,
                right->width - self_amount, right->height);
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
            space = MIN(space, 0);
            self_amount = MAX(amount, space);
            if (space > amount) {
                /* try to get more space by pushing the top edge */
                space -= bump_frame_edge(frame, FRAME_EDGE_TOP,
                        -amount + space);
                amount = MAX(amount, space);
            } else {
                amount = self_amount;
            }
        } else {
            get_minimum_frame_size(right, &size);
            space = right->height - size.height;
            space = MAX(space, 0);
            self_amount = MIN(amount, space);
            if (space < amount) {
                /* try to get more space by pushing the bottom edge */
                space += bump_frame_edge(right, FRAME_EDGE_BOTTOM,
                        amount - space);
                amount = MIN(amount, space);
            } else {
                amount = self_amount;
            }
        }

        resize_frame_and_ignore_ratio(frame, frame->x, frame->y,
                frame->width, frame->height + self_amount);
        resize_frame_and_ignore_ratio(right, right->x, right->y + self_amount,
                right->width, right->height - self_amount);
        break;
    }

    /* make the size change known to all parents */
    const frame_split_direction_t direction = edge == FRAME_EDGE_RIGHT ?
            FRAME_SPLIT_HORIZONTALLY : FRAME_SPLIT_VERTICALLY;
    propagate_size(frame, direction);
    propagate_size(right, direction);

    return amount;
}

/* Count the frames in horizontal direction. */
static unsigned count_horizontal_frames(Frame *frame)
{
    unsigned left_count, right_count;

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
static unsigned count_vertical_frames(Frame *frame)
{
    unsigned left_count, right_count;

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
    unsigned left_count, right_count;

    /* check if the frame has any children */
    if (frame->left == NULL) {
        return;
    }

    if (direction == frame->split_direction) {
        switch (direction) {
        case FRAME_SPLIT_HORIZONTALLY:
            left_count = count_horizontal_frames(frame->left);
            right_count = count_horizontal_frames(frame->right);
            frame->left->width = (uint64_t) frame->width * left_count /
                (left_count + right_count);
            frame->right->x = frame->x + frame->left->width;
            frame->right->width = frame->width - frame->left->width;
            break;

        case FRAME_SPLIT_VERTICALLY:
            left_count = count_vertical_frames(frame->left);
            right_count = count_vertical_frames(frame->right);
            frame->left->height = (uint64_t) frame->height * left_count /
                (left_count + right_count);
            frame->right->y = frame->y + frame->left->height;
            frame->right->height = frame->height - frame->left->height;
            break;
        }
    }

    equalize_frame(frame->left, direction);
    equalize_frame(frame->right, direction);
}
