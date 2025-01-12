#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#include <xcb/xcb.h>

/* the number the first window gets assigned */
#define FIRST_WINDOW_NUMBER 1

typedef struct window {
    /* the actual X window */
    xcb_window_t xcb_window;
    /* if the window is visible */
    unsigned visible : 1;
    /* if the window has focus */
    unsigned focused : 1;
    /* the id of this window */
    uint32_t number;
    /* the next window in the linked list */
    struct window *next;
} Window;

/* the first window in the linked list */
extern Window *g_first_window;

typedef struct frame {
    /* the window inside the frame, may be NULL */
    Window *window;
    /* coordinates and size of the frame */
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    /* the next frame in the linked list */
    struct frame *next;
} Frame;

/*the empty Frame is a frame without a window*/
typedef struct empty_frame {
    /* coordinates and size of the frame */
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    /* the next frame in the linked list */
    struct empty_frame *next;
} EmptyFrame;

/* the first frame in the linked list */
extern Frame *g_first_frame;
/* the currently selected/focused frame */
extern Frame *g_cur_frame;

/* the first empty frame in the linked list */
extern EmptyFrame *g_first_empty_frame;

/* create a window struct and add it to the window list,
 * this also assigns the next id */
Window *create_window(xcb_window_t xcb_window);

/* get the frame this window is contained in, may return NULL */
Frame *get_frame_of_window(Window *window);

/* shows the window by mapping and sizing it */
void show_window(Window *window);

/* hides the window by unmapping it */
void hide_window(Window *window);

/* gets the currently focused window */
Window *get_focus_window(void);

/* set the window that is in focus */
void set_focus_window(Window *window);

/* create a frame at given coordinates that contains a window (`win` may be 0)
 * and attach it to the linked list */
Frame *create_frame(Window *window, int32_t x, int32_t y, int32_t w, int32_t h);

/* create an empty frame at given coordinates and attach it to the linked list */
EmptyFrame *create_empty_frame(int32_t x, int32_t y, int32_t w, int32_t h);

/* assign a window to an empty frame, converting it to a regular frame */
Frame *assign_window_to_empty_frame(EmptyFrame *empty_frame, Window *window);

/* remove a frame from the screen and hide the inner window, this
 * returns 1 when the given frame is the last frame */
int remove_frame(Frame *frame);

/* set the frame in focus, this also focuses the inner window if it exists */
void set_focus_frame(Frame *frame);

/*get the frame in focus*/
Frame *get_focus_frame(void);

/* reload the frame to apply changes with xcb */
void reload_frame(Frame *frame);

#endif
