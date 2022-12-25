# Sample: Jailbreak PRX (OOSDK v0.5.2)

## Directory structure
```
Jailbreak PRX
|-- src
        |- build                         // Object files / intermediate directory
    |-- multi-jb.c                         // all the Jailbreak code for HEN
    |-- multi-jb.h                         // Jailbreak Offsets
|-- jb.prx                                 // final library file (not present until built)
|-- Makefile                               // Make rules for building on Linux
```
The ELF, Orbis ELF (OELF), and object files will all be stored in the intermediate directory `src/build`. The final library prx and stub files will be found in the root directory.



## Libraries used

- Musl (-lc -lc++)
- libkernel



## Building

On Linux, a makefile has been included.

To build this project, the developer will need clang, which is provided in the toolchain. The `OO_PS4_TOOLCHAIN` environment variable will also need to be set to the root directory of the SDK installation.


__Linux__
Run the makefile.
```
make
```

## Reference(s) used

Libjbc - https://github.com/sleirsgoevy/ps4-libjbc


## Author(s)
- LM
- Sleirsgoevy
- Specter
