#ifndef STASH_FRAME_H
#define STASH_FRAME_H

#include "bits/frame.h"

/* Take frame away from the screen, this leaves a singular empty frame.
 *
 * @frame is made into a completely empty frame as all children and windows are
 *        taken out.
 *
 * Consider using `link_into_stash()` after calling this.
 *
 * @return may be NULL if the frame is not worth stashing.
 */
Frame *stash_frame_later(Frame *frame);

/* Links a frame into the stash linked list.
 *
 * The stash object also gets linked into the frame number list as frame object.
 *
 * @frame may be NULL, then nothing happens.
 *
 * Use this on frames returned by `stash_frame_later()`.
 */
void link_frame_into_stash(Frame *frame);

/* Take frame away from the screen, hiding all inner windows and leaves a
 * singular empty frame.
 *
 * This is a simple wrapper around `stash_frame_later()` that calls
 * `link_into_stash()` immediately.
 *
 * @return the stashed frame, there is actually no reason to do anything with
 *         this beside checking if it's NULL.
 */
Frame *stash_frame(Frame *frame);

/* Unlinks given @frame from the stash linked list.
 *
 * This allows to pop arbitrary frames from the stash and not only the last
 * stashed frame.
 *
 * @frame must have been stashed by a previous call to `stash_frame_later()`.
 */
void unlink_frame_from_stash(Frame *frame);

/* Pop a frame from the stashed frame list.
 *
 * The caller may use `replace_frame()` with this frame and then destroy it.
 *
 * @return NULL when there are no stashed frames.
 */
Frame *pop_stashed_frame(void);

/* Puts a frame from the stash into given @frame.
 *
 * @frame must be empty with no windows or children.
 */
void fill_void_with_stash(Frame *frame);

#endif
