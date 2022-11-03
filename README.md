# BD
Simple (to use), one-file build tool for C projects.

## How to use
1. Put [`bd.c`](src/bd.c) into the root of your project, [configure](#how-to-configure) settings if needed
2. Run `gcc -Wall -o bd bd.c` or `gcc -Wall -o bd bd.c; ./bd`

## Help / Command line interface
To see a list of all available commands and their description, run `./bd -h`

## Colors
I picked colors for the following states:
- cyan = compiling
- yellow = linking
- green = up to date

## How to configure
### Types of projects (`Prj::type`)
- `BUILD_APP` builds an executable
- `BUILD_STATIC` builds a static library
- `BUILD_SHARED` builds a shared library
- `BUILD_EXAMPLES` same as app, but it links each specified [file]()
### Name of the project (`Prj::name`)
Define a name for your output file. **If you're building a library**, precede the name with `lib`. If you're building examples, the name is ignored, instead it create the name based off the specified file.
### Object directory (`Prj::objd`)
In this folder all the object files will be dumped. Also, this folder will be deleted completely when cleaning.
### Source files (`Prj::srcf`)
Specify source files to take into account. It's recommended to only use `*` and `?` for globbing to support all operating systems.
### C compile flags (`Prj::cflgs`)
String with your own flags. It's recommended to always at least include `-Wall`
### Linker options (`Prj::lopts`)
String with your own linker options.
### Linker libraries (`Prj::llibs`)
String with your own linker libraries. **Precede paths** with `-L=` and **precede names** itself with `-l=`.
