#include "frame.h"
#include "window.h"

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

/* Check if @window still exists as hidden tiling window.
 *
 * @window may be NULL or a completely random memory address and this function
 *         can still handle that.
 */
static bool is_window_valid(Window *window)
{
    for (Window *other = Window_first; other != NULL; other = other->next) {
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

/* Take @frame away from the screen, this leaves a singular empty frame. */
Frame *stash_frame_later(Frame *frame)
{
    /* check if it is worth saving this frame */
    if (is_frame_void(frame) && frame->number != 0) {
        return NULL;
    }

    Frame *const stash = xcalloc(1, sizeof(*stash));
    stash->number = frame->number;
    frame->number = 0;
    /* reparent the child frames */
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

    hide_inner_windows(stash);
    return stash;
}

/* Links a frame into the stash linked list. */
void link_frame_into_stash(Frame *frame)
{
    if (frame == NULL) {
        return;
    }
    frame->previous_stashed = Frame_last_stashed;
    Frame_last_stashed = frame;
}

/* Take @frame away from the screen, this leaves a singular empty frame. */
Frame *stash_frame(Frame *frame)
{
    Frame *const stash = stash_frame_later(frame);
    if (stash == NULL) {
        return NULL;
    }
    link_frame_into_stash(stash);
    return stash;
}

/* Unlinks given @frame from the stash linked list. */
void unlink_frame_from_stash(Frame *frame)
{
    Frame *previous;

    if (Frame_last_stashed == frame) {
        Frame_last_stashed = Frame_last_stashed->previous_stashed;
    } else {
        previous = Frame_last_stashed;
        while (previous->previous_stashed != frame) {
            previous = previous->previous_stashed;
        }
        previous->previous_stashed = frame->previous_stashed;
    }

    (void) validate_inner_windows(frame);
    show_inner_windows(frame);
}

/* Frees @frame and all child frames. */
static void free_frame_recursively(Frame *frame)
{
    if (frame->left != NULL) {
        free_frame_recursively(frame->left);
        free_frame_recursively(frame->right);
    }
    destroy_frame(frame);
}

/* Put the child frames or window into @frame of the recently saved frame. */
Frame *pop_stashed_frame(void)
{
    Frame *pop = NULL;

    /* find the first valid frame in the pop list, it might be that a stashed
     * frame got invalidated because it lost all inner window and is now
     * completely empty
     */
    while (Frame_last_stashed != NULL) {
        if (validate_inner_windows(Frame_last_stashed) > 0 ||
                Frame_last_stashed->number > 0) {
            break;
        }

        Frame *const free_me = Frame_last_stashed;
        Frame_last_stashed = Frame_last_stashed->previous_stashed;
        free_frame_recursively(free_me);
    }

    if (Frame_last_stashed != NULL) {
        pop = Frame_last_stashed;
        Frame_last_stashed = Frame_last_stashed->previous_stashed;
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
    destroy_frame(pop);
}
