# norbit-stx-vis

## Prerequisites:

Install: OpenGL, GLEW, SDL2

Arch linux:
```
pacman -S mesa glew sdl2
```

ubinit

## Build:
```
make
```

## Run:
```
./run <-f <file>|-t <tcp_ip>:<port>> -m <raw|sbd>


# raw tcp stream
./run -t 192.168.3.121:2210 -m raw

# raw file
./run -f sonar-dump.bin -m raw

# sbd file
./run -f in.sbd -m sbd

```
