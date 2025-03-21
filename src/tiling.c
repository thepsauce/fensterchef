#include <inttypes.h>

#include "configuration.h"
#include "log.h"
#include "size_frame.h"
#include "stash_frame.h"
#include "utility.h"
#include "window.h"

/* Split a frame horizontally or vertically. */
void split_frame(Frame *split_from, Frame *other, bool is_left_split,
        frame_split_direction_t direction)
{
    Frame *new;

    new = create_frame();
    if (other == NULL) {
        other = create_frame();
        if (configuration.tiling.auto_fill_void) {
            fill_void_with_stash(other);
        }
    }

    new->number = split_from->number;
    split_from->number = 0;
    /* let `new` take the children or window */
    if (split_from->left != NULL) {
        new->split_direction = split_from->split_direction;
        new->ratio = split_from->ratio;
        new->left = split_from->left;
        new->right = split_from->right;
        new->left->parent = new;
        new->right->parent = new;
    } else {
        new->window = split_from->window;
        split_from->window = NULL;
    }

    split_from->split_direction = direction;
    /* ratio of 1/2 */
    split_from->ratio.numerator = 1;
    split_from->ratio.denominator = 2;
    if (is_left_split) {
        split_from->left = other;
        split_from->right = new;
    } else {
        split_from->left = new;
        split_from->right = other;
    }
    new->parent = split_from;
    other->parent = split_from;

    if (split_from == Frame_focus) {
        Frame_focus = new;
    }

    /* size the child frames */
    resize_frame(split_from, split_from->x, split_from->y,
            split_from->width, split_from->height);
    if (configuration.tiling.auto_equalize) {
        apply_auto_equalize(split_from);
    }

    LOG("split %F(%F, %F)\n", split_from, new, other);
}

/* Remove a frame from the screen. */
void remove_frame(Frame *frame)
{
    Frame *parent, *other;

    if (frame->parent == NULL) {
        /* this case should always be handled by the caller */
        LOG_ERROR("can not remove the root frame %F\n", frame);
        return;
    }

    parent = frame->parent;
    /* disconnect the frame */
    frame->parent = NULL;

    if (frame == parent->left) {
        other = parent->right;
    } else {
        other = parent->left;
    }

    parent->number = other->number;
    other->number = 0;
    parent->left = other->left;
    parent->right = other->right;
    if (other->left != NULL) {
        /* pass through the children */
        parent->split_direction = other->split_direction;
        parent->ratio = other->ratio;
        parent->left->parent = parent;
        parent->right->parent = parent;

        other->left = NULL;
        other->right = NULL;
    } else {
        parent->window = other->window;

        other->window = NULL;
    }
    /* disconnect `other`, it will be destroyed later */
    other->parent = NULL;

    /* reload all child frames */
    resize_frame(parent, parent->x, parent->y, parent->width, parent->height);

    LOG("frame %F was removed\n", frame);

    /* do not leave behind broken focus */
    if (Frame_focus == frame || Frame_focus == other) {
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
}
