#!/bin/sh

CC=cc
CFLAGS='-std=c99 -Iinclude -Wall -Wextra -O3 -DNO_ANSII_COLORS'
LDLIBS=
# Packages (added to CFLAGS and LDLIBS before compiling/linking)
PACKAGES='x11 xrandr xcursor xft'
PREFIX=/usr
SOURCES=src/*.c\ src/utility/*.c\ src/configuration/*.c

rebuild=false

_command() {
    echo "$@"
    "$@" || exit 1
}

fensterchef() {
    if ! $rebuild && [ -f fensterchef ] ; then
        echo 'fensterchef has been built already'
    else
        CFLAGS="$CFLAGS `pkg-config --cflags $PACKAGES`"
        LDLIBS="$LDLIBS `pkg-config --libs $PACKAGES`"
        _command "$CC" $LDFLAGS $CFLAGS $SOURCES -o fensterchef $LDLIBS
    fi
}

install() {
    fensterchef
    _command mkdir -p "$PREFIX/share/licenses"
    _command mkdir -p "$PREFIX/bin"
    _command mkdir -p "$PREFIX/share/man/man1"
    _command mkdir -p "$PREFIX/share/man/man5"
    _command cp LICENSE.txt "$PREFIX/share/licenses"
    _command cp fensterchef "$PREFIX/bin"
    _command gzip --best -c doc/fensterchef.1 >"$PREFIX/share/man/man1"
    _command gzip --best -c doc/fensterchef.5 >"$PREFIX/share/man/man5"
}

uninstall() {
    license="$PREFIX/share/licenses/fensterchef/"
    if [ -d "$license" ] ; then
        _command rm -rf "$license"
    fi

    for f in "$PREFIX/bin/fensterchef" \
             "$PREFIX/share/man/man1/fensterchef.1" \
             "$PREFIX/share/man/man1/fensterchef.5" ; do
        if [ -f "$f" ] ; then
            _command rm -f "$f"
        fi
    done
}

clean() {
    if [ -f fensterchef ] ; then
        _command rm fensterchef
    fi
}

if [ "$1" = "-B" ] ; then
    rebuild=true
    shift
fi

if [ $# -eq 0 ] ; then
    fensterchef
    exit 0
fi

while [ $# -ne 0 ] ; do
    case "$1" in
    # Targets
    fensterchef) fensterchef ;;
    install) install ;;
    uninstall) uninstall ;;
    clean) clean ;;
    # Overriding/appending of variables
    CC=*) CC=`expr "$1" : 'CC=\(.*\)'` ;;
    CFLAGS=*) CFLAGS=`expr "$1" : 'CFLAGS=\(.*\)'` ;;
    CFLAGS+=*) CFLAGS="$CFLAGS `expr "$1" : 'CFLAGS+=\(.*\)'`" ;;
    LDLIBS=*) LDLIBS=`expr "$1" : 'LDLIBS=\(.*\)'` ;;
    LDLIBS+=*) LDLIBS="$LDLIBS `expr "$1" : 'LDLIBS+=\(.*\)'`" ;;
    PACKAGES=*) PACKAGES=`expr "$1" : 'PACKAGES=\(.*\)'` ;;
    PACKAGES+=*) PACKAGES="$PACKAGES `expr "$1" : 'PACKAGES+=\(.*\)'`" ;;
    PREFIX=*) PREFIX=`expr "$1" : 'PREFIX=\(.*\)'` ;;
    SOURCES=*) SOURCES=`expr "$1" : 'SOURCES=\(.*\)'` ;;
    SOURCES+=*) SOURCES="$SOURCES `expr "$1" : 'SOURCES+=\(.*\)'`" ;;
    *)
        echo "what is \"$1\"?"
        echo "Usage: $0 [VARIABLE[+]=VALUE]... TARGET"
        echo 'Targets: fensterchef install uninstall clean'
        echo 'Variables: CC CFLAGS LDLIBS PACKAGES PREFIX SOURCES'
        exit 1
        ;;
    esac
    shift
done
