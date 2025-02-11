#ifndef TILING_H
#define TILING_H

#include "frame.h"

/* Split a frame horizontally or vertically. 
 *
 * @is_split_vert true for a vertical split and false for a horizontal split.
 */
void split_frame(Frame *split_from, bool is_split_vert);

/* Remove a frame from the screen and hide the inner windows.
 *
 * @return ERROR when the given frame is the last frame, otherwise OK.
 */
int remove_frame(Frame *frame);

/* Destroy a frame and all child frames and hide all inner windows. */
void abandon_frame(Frame *frame);

#endif
