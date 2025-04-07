# Contributing to fensterchef

Contributing in any shape or form is accepted and we can help you clean up your
code if it is implementing something sensible.

## Goals

The goals of fensterchef are:
- Sensible default behavior with high customizability
- Minimize server requests
- Low CPU usage
- Fast startup time
- Fit as many sizes with minimal code

## Directories

- `src/` contains the `.c` files that implement all functionality
- `include/` contains the `.h` files for any declarations
- `include/bits/` contains small header files mostly used to avoid conflicts
- `generate/` includes code to automatically generate part of the code and
  documentation
- `man/` contains the manual pages
- `images/` contains images showcasing possible fensterchef setups

## Getting started

To start understanding the entire source code, we recommend these steps (read
all steps first before going into it):

### General

- Clone the repository using `git clone
  https://github.com/thepsauce/fensterchef`
- Install `Xephyr`
- Build and run the program using `make sandbox`
- Use `Ctrl+Shift` to let `Xephyr` grab the keyboard and use some key bindings,
  try to open a few terminals (`Super+Return`) and try to use the window list
  to view a list of all windows (`Super+w`)
- Use `Super+Ctrl+Shift+e` to quit
- Investigate the log to see the inner workings of fensterchef
- Go to `src/main.c` which shows the entire startup sequence of fensterchef
- Note that `.h` files always have a verbose description of a function whereas
  the descriptions in `.c` files are shortened, whenever I say "Go to
  `src/XXX.c`", you should also check out `include/XXX.h`
- Have a basic understanding of the X window system, I recommend reading:
  <https://tronche.com/gui/x/xlib/introduction/>

### Window management

An `FcWindow` (Fensterchef window) is a wrapper around an `XClient`. The
`XClient` is a wrapper around a raw X window and caches a few properties like
position and size.

Windows were designed to be a very easy to use abstration. You can do for
example `window->x = 8` and this will automatically synchronize the window X
position with the X server on the next event cycle.

- Go to `src/event.c` to see how a few X events are handled, most importantly:
  - `handle_map_request()`
  - `handle_property_notify()`
- Go to `src/window.c` to see how we wrap around X windows
- Go to `src/window_properties.c` to see how window modes are determined based
  off the X atoms and how the X atoms are kept in sync with the X server
- Go to `src/window_state.c` to see how the different window modes like tiling,
  floating etc. are implemented

### Frame management

Frames are used to partition the screen space. Each frame can be split into two
sub frames. The parent remains and is wrapped around the children. The children
share the parent space.

- Remember how `src/window_state.c` shows and hides tiling windows
- Go to `src/frame.c` to see how frames are set up, using `create_frame()` gets
  a completely usable and valid frame and no further setup is needed
- All other functions are helper functions to manage frames, refer to the
  comments describing them in `include/frame.h`
- Go to `src/split_frame.c` to see how frames are split in two and unsplitted
- Note that unlike windows, doing `frame->x = 8` visibly does not change
  anything. In addition, you must call `reload_frame(frame)` to also size the
  inner window. To size the inner children, use `resize_frame(frame, x, y,
  width, height)` from `include/size_frame.h`
- Go to `src/size_frame.c` to see how frames are size and how when parents
  are size, recursively resize their chilrend
- Go to `src/move_frame.c` to see how frames are moved around the tiling layout
  and how we get from one frame to another
- Go to `src/stash_frame.c` to see how frames are stashed, stashed frames are
  simply off-screen frames

### X client management

Calling any functions (besides changing the stacking order) does not synchronize
with the X server. This approach was chosen so that user scripts can interact
effortlessly (without any back and forth) with fensterchef.

- Go to `src/x11_management.c`. Here the connection is initialized to the X
  server, we use the convenient `XkbOpenDisplay()` which does some Xkb version
  checks for us
- Go back to `src/event.c` and look at `next_cycle()`, this is the heart of
  the event loop and synchronizes all that has happened with the X server using
  `synchronize_with_server()`.

### Configuration

TODO as a heavy refactoring is in order

### Miscalleneous

These are files used to extend fensterchef and implement core utility features

#### Utility

- Go to `src/utility.c` or `src/xalloc.c` to see a lot of helpful utility that
  is independent of fensterchef

#### Logging

- Go to `src/log.c` to see the logging utility, we provide `log_formatted()`
  as an extension to `printf()`, it is used through the `LOG*` macros

#### Font drawing

- Go to `src/font.c` to see how text objects are created and rendered
- Xft is used for drawing fonts, we implemented multiple font support on top of
  it

#### Cursor management

- Go to `src/cursor.c` to see how cursors are loaded and cached
- Xcursor is used to load cursors and we simply cache them here using a simple
  hashmap

#### Window list

- Go to `src/window_list` to see how the window list is initialized on demand
  and how it behaves

#### Notification window

- Go to `src/notification.c` to see how notifications are displayed

#### Program options

- Go to `src/program_options.c` to see what program options are handled. There
  is a simple parser for the command line

## Notable files

- `gmon.out` is generated when running the build. It contains code profiling,
  you can use `gprof build/fensterchef gmon.out >profile.txt` to get the
  profiling information
- `README.md` contains a short and concise introduction that should fit on a
  single page
- `CONTRIBUTING.md` is me, I tell you how you can contribute to the project
- `generate/fensterchef.data_types` contains the data types the configuration
  parser understands
- `generate/fensterchef.labels` contains the sections the configuration parser
  understands
- `generate/Makefile` is the make file to update the code and documentation, use
  it with `make -f generate/Makefile`
- `generate/*.{c,h}.sh` are all the files used for code generation, they have
  a form resembling the file they edit
- `man/fensterchef.1` is the main manual to inform the user what fensterchef is
  about and how it can be used
- `man/fensterchef.5` contains information about the configuration file format
  and how configuration files can be written as well as examples

### Makefile

We use `make` for building.

The `Makefile` is the main tool for development and installation, it has the
following "targets":

- `build` builds the entire project and puts the object files in `build/` and
the executable in `build/fensterchef`
- `sandbox` builds and starts `Xephyr` with fensterchef running
- `release` makes the release build which will be in `release/`.
  Note that this can catch additional code warnings in some cases
- `install` installs fensterchef on the system (needs root privileges)
- `uninstall` removes all fensterchef files from the system (needs root
  privileges)
- `clean` removes the `build/` and `release/` directory

## What now

You should have a good understanding of fensterchef now and you can try to
implement your own functionality. Fensterchef documents well how functions
should be used in the header files and what is not allowed. We followed the
philosophy that you can not break fensterchef using any amount of global
functions in any order making it easy to write code and test it.
There are some expections to this case and these are always documented. Usually
it is just that you have to wait until some parts have been initialized in
`main()`.
