# Fensterchef – The X11 Tiling Window Manager for Linux

Fensterchef is a lightweight, lightning-fast window manager for Linux, focused on manual tiling.

🔹 Manual Tiling: Arrange your windows exactly how you want — no rigid grids or enforced layouts. </br>
🔹 Lightweight & Fast: Minimal overhead ensures smooth performance, even on low-end hardware. </br>
🔹 Highly Customizable: Configure Fensterchef easily with a simple configuration file. </br>
🔹 Keyboard-Centric: Navigate your workspace effortlessly with intuitive shortcuts. </br>

## Gallery

I am a boring person when it comes to customizing but this is how it might look:

![fensterchef](./images/fensterchef.png)
![fensterchef](./images/fensterchef2.png)

## Installation

Get started immediately! Open a terminal and clone the repository:
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

Now you have the **fensterchef** executable (`/usr/bin/fensterchef`)
and the two manual pages (`/usr/share/man/man1/fensterchef.1.gz` and `/usr/share/man/man5/fensterchef.5.gz`).

If you are using a login manager, you can simply put this at the end of your `~/.xsession`:
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

## Bugs

Report any issues directly to us over the Github issues tab.

An issue should start with the version, the rest is up to you. Try to add steps
to reproduce the issure and add the relevant excerpts from the log.
