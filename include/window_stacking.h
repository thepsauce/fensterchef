#ifndef WINDOW_STACKING_H
#define WINDOW_STACKING_H

/**
 * Handle the stacking of windows and the synchronization with the X server.
 */

#include "bits/window.h"

#include "utility/utility.h"

/* Remove @window from the Z linked list. */
void unlink_window_from_z_list(FcWindow *window);

/* Links the window into the z linked list at a specific place.
 *
 * The window should have been unlinked from the Z linked list first.
 *
 * @below may be NULL in which case the window is inserted at the bottom.
 */
void link_window_above_in_z_list(FcWindow *window, _Nullable FcWindow *below);

/* Remove @window from the Z server linked list. */
void unlink_window_from_z_server_list(FcWindow *window);

/* Links the window into the z linked list at a specific place.
 *
 * The window should have been unlinked from the Z server linked list first.
 *
 * @below must NOT be NULL.
 */
void link_window_above_in_z_server_list(FcWindow *window,
        _Nonnull FcWindow *below);

/* Put the window on the best suited Z stack position. */
void update_window_layer(FcWindow *window);

/* Synchronize the window stacking order with the server. */
void synchronize_window_stacking_order(void);

#endif
