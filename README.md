# Fensterchef – The X11 Tiling Window Manager

Fensterchef is a keyboard-centric tiling window manager for unix system using
X11.

## Installation

### Dependencies

X11 Xrandr Xcursor Xft fontconfig

### Build from source

You will need: coreutils, pkgconf, any C99 compiler and a unix shell

```sh
git clone https://github.com/fensterchef/fensterchef.git
sudo ./make install
fensterchef --version
```

### Setup

If you are using a login manager, you can simply put this at the end of your
`~/.xsession`:
```
mkdir -p ~/.local/share/fensterchef
exec /usr/bin/fensterchef -dinfo 2>~/.local/share/fensterchef
```
Alternatively put it into the `~/.xinitrc`.

*How to get fensterchef to run exactly varies on your environment.*

### Try it out in the sandbox

If you have Xephyr installed you can (after cloning the repository) do:
```sh
make -f build/Makefile sandbox
```
This will open a 800x600 window in your current desktop environment in which you
can try out **fensterchef**.

### [Further reading](https://fensterchef.org)

## Contact

- Casual IRC chat: `irc.libera.chat:6697` in `#fensterchef`
- Questions: [Github discussions](https://github.com/fensterchef/fensterchef/discussions)
- Bugs/Feature requests: [Github issues](https://github.com/fensterchef/fensterchef/issues)
- Other: fensterchef*@*fensterchef*.*org
