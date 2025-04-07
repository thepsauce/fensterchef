# Fensterchef – The X11 Tiling Window Manager

Fensterchef is a keyboard-centric tiling window manager for X11.

## Installation

### Dependencies

#### Make dependencies

- make
- pkg-config
- find
- (Optionally) Xephyr

#### Library dependencies

- X11
- Xrandr
- Xcursor
- Xft
- fontconfig
- freetype2

### Build from source

Open a terminal and clone the repository:
```sh
git clone https://github.com/thepsauce/fensterchef.git
```
(Optional) If you want to check out the currently developing features:
```sh
git checkout developing
```
Then simply type the following and enter your password.
```sh
sudo make install
```

Now you have the **fensterchef** executable (`/usr/bin/fensterchef`) and the two
manual pages (`/usr/share/man/man1/fensterchef.1.gz` and
`/usr/share/man/man5/fensterchef.5.gz`).

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
make sandbox
```
This will open a 800x600 window in your current desktop environment in which you
can try out **fensterchef**.

### What now

Check out the Wiki on Github!

## Contact

If you want to hang out with fensterchef users and developers you can join
`irc.libera.chat:6697` and find us in the channel named `#fensterchef`.

Alternatively, for forum-like discussion, I created a subreddit on Reddit:
https://reddit.com/r/fensterchef
