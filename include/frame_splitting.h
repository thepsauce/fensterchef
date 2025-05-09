#ifndef SPLIT_H
#define SPLIT_H

#include "bits/frame.h"

/* Split a frame horizontally or vertically.
 *
 * @split_from is the frame to split a frame off of.
 * @other is put into the the slot created by the split. It may be NULL, then
 *        one is allocated by this function and filled using the stash if
 *        auto-fill-void is configured.
 * @is_left_split controls where @other goes together with @direction,
 *                either on the top/left or bottom/right.
 */
void split_frame(Frame *split_from, Frame *other, bool is_left_split,
        frame_split_direction_t direction);

/* Remove a frame from the screen.
 *
 * This keeps the children within @frame in tact meaning they are not destroyed
 * or stashed. This needs to be done externally.
 * Stashing the frame before removing it is a good approach.
 *
 * @frame shall not be a root frame.
 */
void remove_frame(Frame *frame);

#endif
