<div align="center">
  <img src="img/logo.png" height="300" width="300"/>
  <h1 align="center">Toolen</h1>
</div>

## Overview

Toolen is a simple GNU/Linux core tool implementation. It includes some of the more standard core features and some homebrew tools.

## Prepare environment

### Required tools & libraries

1. **gcc**: GCC Version 14.0.0+ is recommended
2. **make**: Used to build projects
3. **ncurses**: Text-based user interface library
4. **clang**: Optional, if you couldn't install gcc

### Installation steps

**Debian/Ubuntu**
```bash
sudo apt update && sudo apt install make gcc libncurses-dev # clang
```

**ArchLinux/Manjaro**
```bash
sudo pacman -Sy make gcc ncurses # clang
```

**Termux**
```bash
pkg update && pkg install make clang libncurses-dev
```

## Build steps

### Clone this project

```bash
git clone https://github.com/ASO-Studio/Toolen
```

### Configuration

```bash
make menuconfig # or make allyesconfig
```

### Compile

```bash
make -j$(nproc)
```

## Usage
```bash
./toolen [command|options] [args...]
```
or
```bash
ln -sf toolen CommandName
./CommandName
# For example:
#  ln -sf toolen echo
#  ./echo "Hello world!"
```

## TODO
- [ ] uname: Print userspace type
- [ ] reboot: Send SIGTERM & SIGKILL to all processes
- [ ] reboot: Force reboot
- [ ] Fix warnings at link-time
