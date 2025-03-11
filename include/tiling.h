#ifndef TILING_H
#define TILING_H

#include "frame.h"

/* Split a frame horizontally or vertically.
 *
 * @frame is put into the right/bottom of the split. It may be NULL, then one is
 *        allocated by this function and filled by the stash.
 */
void split_frame(Frame *split_from, Frame *right,
        frame_split_direction_t direction);

/* Note for all `get_XXX_frame()` functions that the frame returned usually is a
 * parent frame, meaning it has children, for example:
 * +------+----+
 * |      | 2  |
 * |  1   +----+
 * |      | 3  |
 * +------+----+
 *
 * (pointers illustrated by above numbers)
 * `get_left_frame(2)` -> 1
 * `get_left_frame(3)` -> 1
 * `get_right_frame(1)` -> frame surrounding 2 and 3
 * `get_above_frame(3)` -> 2
 * `get_above_frame(2)` -> NULL
 */

/* Get the frame on the left of @frame. */
Frame *get_left_frame(Frame *frame);

/* Get the frame above @frame. */
Frame *get_above_frame(Frame *frame);

/* Get the frame on the right of @frame. */
Frame *get_right_frame(Frame *frame);

/* Get the frame below @frame. */
Frame *get_below_frame(Frame *frame);

/* Increases the @edge of @frame by @amount. */
int32_t bump_frame_edge(Frame *frame, frame_edge_t edge, int32_t amount);

/* Set the size of all children of @frame to be equal. */
void equalize_frame(Frame *frame);

/* Remove an empty frame from the screen.
 *
 * @return ERROR when the given frame is a root frame, otherwise OK.
 */
int remove_void(Frame *frame);

#endif
