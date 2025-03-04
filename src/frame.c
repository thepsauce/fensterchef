#include <inttypes.h>

#include "configuration.h"
#include "fensterchef.h"
#include "frame.h"
#include "log.h"
#include "monitor.h"
#include "utility.h"
#include "window.h"

/* the currently selected/focused frame */
Frame *focus_frame;

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

    for (Monitor *monitor = first_monitor; monitor != NULL;
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
        /* keep ration when resizing or use the default 1/2 ratio */
        left_size = left->width == 0 || right->width == 0 ? width / 2 :
            width * left->width / (left->width + right->width);
        resize_frame(left, x, y, left_size, height);
        resize_frame(right, x + left_size, y, width - left_size, height);
        break;

    /* top to bottom split */
    case FRAME_SPLIT_VERTICALLY:
        /* keep ratio when resizing or use the default 1/2 ratio */
        left_size = left->height == 0 || right->height == 0 ? height / 2 :
            height * left->height / (left->height + right->height);
        resize_frame(left, x, y, width, left_size);
        resize_frame(right, x, y + left_size, width, height - left_size);
        break;
    }
}

/* Replace @frame (windows and child frames) with @with. */
void replace_frame(Frame *frame, Frame *with)
{
    /* reparent the child frames */
    if (with->left != NULL) {
        frame->split_direction = with->split_direction;
        frame->left = with->left;
        frame->right = with->right;
        frame->left->parent = frame;
        frame->right->parent = frame;

        with->left = NULL;
        with->right = NULL;
    } else {
        frame->window = with->window;

        with->window = NULL;
    }

    /* reload the frame recursively */
    resize_frame(frame, frame->x, frame->y, frame->width, frame->height);
}

/* Resize the inner window to fit within the frame. */
void reload_frame(Frame *frame)
{
    Frame *root;
    uint32_t left, top, right, bottom;

    if (frame->window == NULL) {
        return;
    }

    root = get_root_frame(frame);
    if (root->x == frame->x) {
        left = configuration.gaps.outer;
    } else {
        left = configuration.gaps.inner;
    }

    if (root->y == frame->y) {
        top = configuration.gaps.outer;
    } else {
        top = configuration.gaps.inner;
    }

    if (root->x + root->width == frame->x + frame->width) {
        right = configuration.gaps.outer;
    } else {
        right = 0;
    }

    if (root->y + root->height == frame->y + frame->height) {
        bottom = configuration.gaps.outer;
    } else {
        bottom = 0;
    }

    right += left + configuration.border.size * 2;
    bottom += top + configuration.border.size * 2;
    set_window_size(frame->window,
            frame->x + left,
            frame->y + top,
            frame->width < right ? 0 : frame->width - right,
            frame->height < bottom ? 0 : frame->height - bottom);
}

/* Set the frame in focus, this also focuses the inner window if possible. */
void set_focus_frame(Frame *frame)
{
    set_focus_window(frame->window);

    focus_frame = frame;

    set_notification(frame->left == NULL ? (utf8_t*) "Current frame" :
            (utf8_t*) "Current frames",
            focus_frame->x + focus_frame->width / 2,
            focus_frame->y + focus_frame->height / 2);

    LOG("frame %F was focused\n", frame);
}

/* Focus @window and the frame it is in. */
void set_focus_window_with_frame(Window *window)
{
    Frame *frame;

    if (window == NULL) {
        set_focus_window(NULL);
        return;
    }

    frame = get_frame_of_window(window);
    if (frame != NULL) {
        set_focus_frame(frame);
    } else {
        set_focus_window(window);
    }
}

/* Get the frame above the given one that has no parent. */
Frame *get_root_frame(Frame *frame)
{
    while (frame->parent != NULL) {
        frame = frame->parent;
    }
    return frame;
}
