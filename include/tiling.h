#ifndef TILING_H
#define TILING_H

#include "frame.h"

/* Fill the frame with the last taken window. */
void fill_empty_frame(Frame *frame);

/* Split a frame horizontally or vertically. */
void split_frame(Frame *split_from, frame_split_direction_t direction);

/* Increases the @edge of @frame by @amount. */
int32_t bump_frame_edge(Frame *frame, frame_edge_t edge, int32_t amount);

/* Remove a frame from the screen and hide the inner windows.
 *
 * @return ERROR when the given frame is the last frame, otherwise OK.
 */
int remove_frame(Frame *frame);

/* Destroy a frame and all child frames and hide all inner windows. */
void abandon_frame(Frame *frame);

#endif
