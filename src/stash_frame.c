#include "frame.h"
#include "window.h"

/* the last frame in the frame stashed linked list */
static Frame *last_stashed_frame;

/* Hide all windows in @frame and child frames. */
static void hide_inner_windows(Frame *frame)
{
    if (frame->left != NULL) {
        hide_inner_windows(frame->left);
        hide_inner_windows(frame->right);
    } else if (frame->window != NULL) {
        hide_window_abruptly(frame->window);
    }
}

/* Take @frame away from the screen, this leaves a singular empty frame. */
Frame *stash_frame_later(Frame *frame)
{
    /* check if it is worth saving this frame */
    if (is_frame_void(frame)) {
        return NULL;
    }

    /* reparent the child frames */
    Frame *const stash = xcalloc(1, sizeof(*stash));
    if (frame->left != NULL) {
        stash->split_direction = frame->split_direction;
        stash->ratio = frame->ratio;
        stash->left = frame->left;
        stash->right = frame->right;
        stash->left->parent = stash;
        stash->right->parent = stash;

        frame->left = NULL;
        frame->right = NULL;
    } else {
        stash->window = frame->window;

        frame->window = NULL;
    }
    return stash;
}

/* Links a frame into the stash linked list. */
void link_frame_into_stash(Frame *frame)
{
    if (frame == NULL) {
        return;
    }
    frame->previous_stashed = last_stashed_frame;
    last_stashed_frame = frame;
}

/* Take @frame away from the screen, this leaves a singular empty frame. */
Frame *stash_frame(Frame *frame)
{
    hide_inner_windows(frame);

    Frame *const stash = stash_frame_later(frame);
    if (stash == NULL) {
        return NULL;
    }
    link_frame_into_stash(stash);
    return stash;
}

/* Check if @window still exists as hidden tiling window.
 *
 * @window may be NULL or a completely random memory address and this function
 *         can still handle that.
 */
static bool is_window_valid(Window *window)
{
    for (Window *other = first_window; other != NULL; other = other->next) {
        if (other == window) {
            return window->state.mode == WINDOW_MODE_TILING &&
                !window->state.is_visible;
        }
    }
    return false;
}

/* Make sure all window pointers are still valid.
 *
 * In theory theory it could happen that a pointer is associated to a different
 * object even when it has the same address. But even if that happened, it would
 * not make this function any worse. The user would not even notice.
 *
 * @return the number of valid windows.
 */
static uint32_t validate_inner_windows(Frame *frame)
{
    if (frame->left != NULL) {
        return validate_inner_windows(frame->left) +
            validate_inner_windows(frame->right);
    } else if (frame->window != NULL) {
        if (!is_window_valid(frame->window)) {
            frame->window = NULL;
            return 0;
        }
        return 1;
    }
    return 0;
}

/* Frees @frame and all child frames. */
static void free_frame_recursively(Frame *frame)
{
    if (frame->left != NULL) {
        free_frame_recursively(frame->left);
        free_frame_recursively(frame->right);
    }
    free_frame(frame);
}

/* Show all windows in @frame and child frames. */
static void show_inner_windows(Frame *frame)
{
    if (frame->left != NULL) {
        show_inner_windows(frame->left);
        show_inner_windows(frame->right);
    } else if (frame->window != NULL) {
        reload_frame(frame);
        frame->window->state.is_visible = true;
    }
}

/* Put the child frames or window into @frame of the recently saved frame. */
Frame *pop_stashed_frame(void)
{
    Frame *pop;

    pop = last_stashed_frame;
    /* find the first valid frame in the pop list, it might be that a stashed
     * frame got invalidated because it lost all inner window and is now
     * completely empty
     */
    while (pop != NULL) {
        if (validate_inner_windows(pop) > 0) {
            break;
        }

        Frame *const free_me = pop;
        pop = pop->previous_stashed;
        free_frame_recursively(free_me);
    }

    if (pop == NULL) {
        last_stashed_frame = NULL;
    } else {
        last_stashed_frame = pop->previous_stashed;
        show_inner_windows(pop);
    }

    return pop;
}

/* Puts a frame from the stash into given @frame. */
void fill_void_with_stash(Frame *frame)
{
    Frame *pop;

    pop = pop_stashed_frame();
    if (pop == NULL) {
        return;
    }
    replace_frame(frame, pop);
    free_frame(pop);
}
