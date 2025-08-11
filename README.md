# Toolen
Toolen is a simple GNU/Linux core tool implementation. It includes some of the more standard core features and some "homebrew tools"

## Prepare environment
**On Ubuntu/Debian**
```bash
sudo apt update && sudo apt install gcc build-essential make
```
**On ArchLinx/Manjaro**
```bash
sudo pacman -Syy gcc build-essential make
```

**On Termux**
```bash
pkg update && pkg install clang build-essential make
```

## Build
```bash
git clone https://github.com/RoofAlan/Toolen && cd Toolen
make
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
- [] uname: Print userspace type
- [] reboot: Send SIGTERM & SIGKILL to all processes
- [] reboot: Force reboot
- [] Fix warnings at link-time
