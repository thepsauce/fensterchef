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
        (uint32_t) (x - frame->x) < frame->width &&
        (uint32_t) (y - frame->y) < frame->height;
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
    uint32_t old_width, old_height;
    uint32_t left_size;

    old_width = frame->width;
    old_height = frame->height;

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
        left_size = old_width == 0 ? width / 2 :
            width * left->width / old_width;
        resize_frame(left, x, y, left_size, height);
        resize_frame(right, x + left->width, y, width - left_size, height);
        break;

    /* top to bottom split */
    case FRAME_SPLIT_VERTICALLY:
        left_size = old_height == 0 ? height / 2 :
            height * left->height / old_height;
        resize_frame(left, x, y, width, left_size);
        resize_frame(right, x, y + left->height, width, height - left_size);
        break;
    }
}

/* Resize the inner window to fit within the frame. */
void reload_frame(Frame *frame)
{
    Monitor *monitor;
    uint32_t left, top, right, bottom;

    if (frame->window == NULL) {
        return;
    }
    monitor = get_monitor_from_rectangle(frame->x, frame->y,
            frame->width, frame->height);
    if (monitor->position.x == frame->x) {
        left = configuration.gaps.outer;
    } else {
        left = configuration.gaps.inner;
    }

    if (monitor->position.y == frame->y) {
        top = configuration.gaps.outer;
    } else {
        top = configuration.gaps.inner;
    }

    if (monitor->position.x + monitor->size.width ==
            frame->x + frame->width) {
        right = configuration.gaps.outer;
    } else {
        right = 0;
    }

    if (monitor->position.y + monitor->size.height ==
            frame->y + frame->height) {
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
    if (frame->window != NULL) {
        set_focus_window(frame->window);
    }
    focus_frame = frame;

    set_notification((utf8_t*) "Current frame",
            focus_frame->x + focus_frame->width / 2,
            focus_frame->y + focus_frame->height / 2);

    LOG("frame %p was focused\n", (void*) frame);
}

/* Get the frame above the given one that has no parent. */
Frame *get_root_frame(Frame *frame)
{
    while (frame->parent != NULL) {
        frame = frame->parent;
    }
    return frame;
}
