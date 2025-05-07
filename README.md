# [Fensterchef](https://fensterchef.org) – The X11 Tiling Window Manager

Fensterchef is a keyboard-centric tiling window manager for unix systems using
X11.  It is meant to be highly configurable both through a custom configuration
language and the source code which is made using clean code principles.

## Installation

### Build from source

Depends on: X11, Xrandr>=1.2, Xcursor and Xft

To build you need: coreutils, pkgconf, any C99 compiler and a unix shell

```sh
git clone https://github.com/fensterchef/fensterchef.git
sudo ./make install
fensterchef --version
```

### Setup

For most display managers like LigthDM, GDM, SLiM etc., it suffices to edit
`~/.xsession`:
```
mkdir -p ~/.local/share/fensterchef
exec /usr/bin/fensterchef -dinfo 2>~/.local/share/fensterchef
```
When using no display manager, use
[your preferred way](https://linux.die.net/man/1/xinit).

## Contact

- Casual IRC chat: `irc.libera.chat:6697` in `#fensterchef`
- Questions: [Github discussions](https://github.com/fensterchef/fensterchef/discussions)
- Bugs/Feature requests: [Github issues](https://github.com/fensterchef/fensterchef/issues)
- Other: fensterchef (at) fensterchef.org
