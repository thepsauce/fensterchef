#ifndef WINDOW_PROPERTIES_H
#define WINDOW_PROPERTIES_H

#include <X11/Xatom.h>

#include "bits/atoms.h"
#include "window_state.h"

/* get the atom identifier from an atom constant */
#define ATOM(id) (x_atom_ids[id])

/* constant list expansion of all atoms */
enum {
#define X(atom) atom,
    DEFINE_ALL_ATOMS
#undef X
    ATOM_MAX
};

/* all X atom names */
extern const char *x_atom_names[ATOM_MAX];

/* all X atom ids */
extern Atom x_atom_ids[ATOM_MAX];

/* Intern all X atoms we require. */
void initialize_atoms(void);

/* Set the initial root window properties. */
void initialize_root_properties(void);

/* Gets the `FENSTERCHEF_COMMAND` property from @window. */
char *get_fensterchef_command_property(Window window);

/* Initialize all properties within @window.
 *
 * @mode is set to the mode the window should be in initially.
 */
void initialize_window_properties(FcWindow *window, window_mode_t *mode);

/* Update the property within @window corresponding to given @atom. */
bool cache_window_property(FcWindow *window, Atom atom);

/* Check if @window supports @protocol. */
bool supports_protocol(FcWindow *window, Atom protocol);

/* Check if @window includes @state. */
bool has_state(FcWindow *window, Atom state);

#endif
