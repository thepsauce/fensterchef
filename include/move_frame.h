#ifndef MOVE_FRAME_H
#define MOVE_FRAME_H

/**
 * This file is all about moving from one frame to another.
 *
 * But also moving a frame from one place to another including exchange with
 * another frame.
 */

#include <stdbool.h>
#include <stdint.h>

#include "bits/frame_typedef.h"

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
 * `get_below_frame(1)` -> NULL
 */

/* Get the frame on the left of @frame. */
Frame *get_left_frame(Frame *frame);

/* Get the frame above @frame. */
Frame *get_above_frame(Frame *frame);

/* Get the frame on the right of @frame. */
Frame *get_right_frame(Frame *frame);

/* Get the frame below @frame. */
Frame *get_below_frame(Frame *frame);

/* Get the most left within @frame.
 *
 * @frame can not be NULL.
 * @y is a hint so that the best frame is picked if more are most left.
 *
 * @return always a leaf frame and never NULL.
 */
Frame *get_most_left_leaf_frame(Frame *frame, int32_t y);

/* Get the frame at the top within @frame.
 *
 * @frame can not be NULL.
 * @x is a hint so that the best frame is picked if more are at the most above.
 *
 * @return always a leaf frame and never NULL.
 */
Frame *get_top_leaf_frame(Frame *frame, int32_t x);

/* Get the most right frame within @frame.
 *
 * @frame can not be NULL.
 * @y is a hint so that the best frame is picked if more are most right.
 *
 * @return always a leaf frame and never NULL.
 */
Frame *get_most_right_leaf_frame(Frame *frame, int32_t y);

/* Get the frame at the bottom within @frame.
 *
 * @frame can not be NULL.
 * @x is a hint so that the best frame is picked if more are at the most bottom.
 *
 * @return always a leaf frame and never NULL.
 */
Frame *get_bottom_leaf_frame(Frame *frame, int32_t x);

/* The `move_frame_XXX()` functions work analogous. Here is an illustration:
 * +-----+---+---+---+
 * |     |   2   |   |
 * |  1  +---+---+ 5 |
 * |     | 3 | 4 |   |
 * +-----+---+---+---+
 *
 * Cases 1-5 (moving frame N to the left):
 * 1. Nothing happens
 * 2. Remove and split to the right of 1
 * 3. Remove and split to the left of 2,4
 * 4. Remove and split to the left of 3
 * 5. Remove and split to the right of 2
 *
 * These special cases are handled as well:
 * S1. A frame is moved into a void
 * S2. Movement across monitors
 */

/* Move @frame to the left.
 *
 * @return if the frame could be moved.
 */
bool move_frame_left(Frame *frame);

/* Move @frame up.
 *
 * @return if the frame could be moved.
 */
bool move_frame_up(Frame *frame);

/* Move @frame to the right.
 *
 * @return if the frame could be moved.
 */
bool move_frame_right(Frame *frame);

/* Move @frame down.
 *
 * @return if the frame could be moved.
 */
bool move_frame_down(Frame *frame);

/* Exchange @from with @to.
 *
 * The exchange must be well defined for the two frames and these cases lead to
 * undefined behavior:
 * - @to is within @from
 * - @from is within @to
 */
void exchange_frames(Frame *from, Frame *to);

#endif
