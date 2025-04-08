#include "frame.h"
#include "window.h"

/* Hide all windows in @frame and child frames. */
static void hide_and_reference_inner_windows(Frame *frame)
{
    if (frame->left != NULL) {
        hide_and_reference_inner_windows(frame->left);
        hide_and_reference_inner_windows(frame->right);
    } else if (frame->window != NULL) {
        hide_window_abruptly(frame->window);
        /* make sure the pointer sticks around */
        reference_window(frame->window);
    }
}

/* Show all windows in @frame and child frames. */
static void show_and_dereference_inner_windows(Frame *frame)
{
    if (frame->left != NULL) {
        show_and_dereference_inner_windows(frame->left);
        show_and_dereference_inner_windows(frame->right);
    } else if (frame->window != NULL) {
        dereference_window(frame->window);
        reload_frame(frame);
        frame->window->state.is_visible = true;
    }
}

/* Make sure all window pointers are still pointing to an existing invisible
 * window.
 *
 * @return the number of valid windows.
 */
static uint32_t validate_inner_windows(Frame *frame)
{
    if (frame->left != NULL) {
        return validate_inner_windows(frame->left) +
            validate_inner_windows(frame->right);
    } else if (frame->window != NULL) {
        if (frame->window->client.id == None ||
                frame->window->state.is_visible) {
            dereference_window(frame->window);
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
    if (is_frame_void(frame) && frame->number == 0) {
        return NULL;
    }

    Frame *const stash = create_frame();
    replace_frame(stash, frame);
    hide_and_reference_inner_windows(stash);
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
    show_and_dereference_inner_windows(frame);
}

/* Frees @frame and all child frames. */
static void free_frame_recursively(Frame *frame)
{
    if (frame->left != NULL) {
        free_frame_recursively(frame->left);
        free_frame_recursively(frame->right);
        frame->left = NULL;
        frame->right = NULL;
    }
    frame->parent = NULL;
    destroy_frame(frame);
}

/* Pop a frame from the stashed frame list. */
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
        show_and_dereference_inner_windows(pop);
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
