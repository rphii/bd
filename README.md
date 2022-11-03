# BD
Simple (to use), one-file build tool for C projects.

## How to use
1. Put [`bd.c`](bd.c) into the root of your project, [configure](#how-to-configure) settings if needed
2. Run `gcc -Wall -o bd bd.c` or `gcc -Wall -o bd bd.c; ./bd`

## Help / Command line interface
To see a list of all available commands and their description, run `./bd -h`. Most important commands:
- `build` build the projects
- `clean` clean the mess
- `clean build` basically requild

## Colors
I picked colors for the following states:
- cyan = compiling
- yellow = linking
- green = up to date
- magenta = cleaning

## How to configure
Modify the template project or add multiple ones. See https://github.com/rphii/Rlib where I created a library and used it to link with examples.
### Types of projects (`Prj::type`)
- `BUILD_APP` builds an executable
- `BUILD_STATIC` builds a static library
- `BUILD_SHARED` builds a shared library
- `BUILD_EXAMPLES` same as app, but it links each specified file
### Name of the project (`Prj::name`)
String of your output file (without extension). **If you're building a library**, precede the name with `lib`. If you're building examples, the name is ignored, instead it create the name based off the specified file(s).
### Object directory (`Prj::objd`)
In this folder all the object (`.o`) and dependency (`.d`) files will be dumped. Also, this folder will be deleted completely when cleaning.
### Source files (`Prj::srcf`)
String-array of source files to take into account. It's recommended to only use `*` and `?` for globbing to support all operating systems.
### C compile flags (`Prj::cflgs`)
String with your own flags. It's recommended to always at least include `-Wall`
### Linker options (`Prj::lopts`)
String with your own linker options.
### Linker libraries (`Prj::llibs`)
String with your own linker libraries. **Precede paths** with `-L=` and **precede names** itself with `-l=`.
