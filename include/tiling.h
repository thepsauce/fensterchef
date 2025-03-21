#ifndef TILING_H
#define TILING_H

#include "bits/frame_typedef.h"

/* Split a frame horizontally or vertically.
 *
 * @split_from is the frame to split a frame off of.
 * @other is put into the the slot created by the split. It may be NULL, then
 *        one is allocated by this function and filled using the stash.
 * @is_left_split controls where @other goes, either on the top/left or
 *                bottom/right.
 */
void split_frame(Frame *split_from, Frame *other, bool is_left_split,
        frame_split_direction_t direction);

/* Remove a frame from the screen.
 *
 * This keeps the children within @frame in tact.
 *
 * @frame shall not be a root frame.
 */
void remove_frame(Frame *frame);

#endif
