#ifndef TILING_H
#define TILING_H

#include "frame.h"

/* Split a frame horizontally or vertically. */
void split_frame(Frame *split_from, frame_split_direction_t direction);

/* Increases the @edge of @frame by @amount. */
int32_t bump_frame_edge(Frame *frame, frame_edge_t edge, int32_t amount);

/* Remove an empty frame from the screen.
 *
 * @return ERROR when the given frame is a root frame, otherwise OK.
 */
int remove_void(Frame *frame);

#endif
