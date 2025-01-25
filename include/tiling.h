#ifndef TILING_H
#define TILING_H

/* Split a frame horizontally or vertically. 
 *
 * @is_split_vert 1 for a vertical split and 0 for a horizontal split.
 */
void split_frame(Frame split_from, int is_split_vert);

/* Remove a frame from the screen and hide the inner window.
 *
 * This frame must have NO children.
 *
 * @return 1 when the given frame is the last frame, otherwise 0.
 */
int remove_leaf_frame(Frame frame);

#endif
