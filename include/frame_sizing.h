#ifndef SIZE_FRAME_H
#define SIZE_FRAME_H

/**
 * This file is all about modifying the size of frames.
 */

#include "frame.h"

/* Apply the auto equalizationg to given frame.
 *
 * This moves up while the split direction matches @direction.
 */
void apply_auto_equalize(Frame *to, frame_split_direction_t direction);

/* Get the minimum size the given frame should have. */
void get_minimum_frame_size(Frame *frame, Size *size);

/* Set the size of a frame, this also resize the child frames and windows. */
void resize_frame(Frame *frame, int x, int y, unsigned width, unsigned height);

/* Set the size of a frame, this also resizes the child frames and windows.
 *
 * This function ignores the @frame->ratio and instead uses the existing ratio
 * between the windows to size them.
 *
 * This function is good for reloading child frames if the parent resized.
 */
void resize_frame_and_ignore_ratio(Frame *frame, int x, int y,
        unsigned width, unsigned height);

/* Increases the @edge of @frame by @amount. */
int bump_frame_edge(Frame *frame, frame_edge_t edge, int amount);

/* Set the size of all children within @frame to be equal within a certain
 * direction.
 */
void equalize_frame(Frame *frame, frame_split_direction_t direction);

#endif
