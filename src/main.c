#include <X11/Xlib.h>

#include <stdio.h>

int main(void)
{
    Display *xdpy;
    Window win;
    XEvent xev;

    xdpy = XOpenDisplay(NULL);
    if (xdpy == NULL) {
        fprintf(stderr, "could not connect to an X11 server\n");
        return -1;
    }

    win = XCreateSimpleWindow(xdpy, DefaultRootWindow(xdpy),
            0, 0, 256, 256, 0, 0, 0);

    XMapWindow(xdpy, win);

    while (XNextEvent(xdpy, &xev) == 0) {
        printf("got event!!\n");
    }

    XDestroyWindow(win);

    XCloseDisplay(xdpy);
    return 0;
}
