#ifndef TILING_H
#define TILING_H

#include "frame.h"

/* Split a frame horizontally or vertically. 
 *
 * The frame @split_from must have NO children.
 *
 * @is_split_vert 1 for a vertical split and 0 for a horizontal split.
 */
void split_frame(Frame *split_from, int is_split_vert);

/* Remove a frame from the screen and hide the inner windows.
 *
 * @return 1 when the given frame is the last frame, otherwise 0.
 */
int remove_frame(Frame *frame);

/* Destroy a frame and all child frames and hide all inner windows.
 */
void abandon_frame(Frame *frame);

#endif
