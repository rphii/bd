# BD
Simple (to use), one-file build tool for C projects.

## What does it do?
- It's a simple build tool for C or C++ projects
- Its instructions on what to build are directly stored within the source code itself, making it compact
- You can choose between four different [build types](#types-of-projects-prjtype)
- It makes sure to recompile a file if their dependency (either header file or library) was modified

## How to use
1. Put [`bd.c`](bd.c) into (the root of) your project, [configure](#how-to-configure) settings if needed
2. Run `gcc -Wall -O2 -o bd bd.c` to compile it or `gcc -Wall -O2 -o bd bd.c; ./bd` to compile and execute
3. Now you can simply run `./bd`, if you don't change your configurations

## Help / command line interface
To see a list of all available commands and their description, run `./bd -h`. Most important commands:
- `build` build the projects (providing no arguments defaults to this)
- `clean` clean the mess
- `clean build` basically rebuild

## Colors
Following colors were picked depending on the action:
- cyan = compiling
- yellow = linking
- green = up to date
- magenta = cleaning

## How to configure
Modify the template project (first instruction in `bd_execute`) or add multiple ones. See https://github.com/rphii/Rlib where I created a library and used it to link with examples.
### Types of projects (`Prj::type`)
- `BUILD_APP` builds an executable
- `BUILD_STATIC` builds a static library
- `BUILD_SHARED` builds a shared library
- `BUILD_EXAMPLES` same as app, but it links each specified file
### Name of the project (`Prj::name`)
String of your output file (without extension).
- **If you're building a library**, precede the name with `lib`
- If you're building examples, the name is treated as a folder name instead
### Object directory (`Prj::objd`)
In this folder all the object (`.o`) and dependency (`.d`) files will be dumped.
### Source files (`Prj::srcf`)
String-array of source files necessary to successfully compile and link the project together. It's recommended to only use `*` and `?` when pattern matching to support all operating systems.
### C compile flags (`Prj::cflgs`)
String with your own flags. It's recommended to always at least include `-Wall`.
### Linker options (`Prj::lopts`)
String with your own linker options.
### Linker libraries (`Prj::llibs`)
String with your own linker libraries.
- **Precede paths** with the `-L=` flag. (make sure to include the equals sign)
- **precede names** with the `-l=` flag. (make sure to include the equals sign)
### C compiler (`Prj::cc`)
String specifying compiler to use.
- If it's `null` it defaults to `gcc`
- Set it to `g++` to compile C++ programs
- Or alternatively use any compiler of your choice

## Minimum Recommended Configuration
### C
```c
Prj p[] = {{
    .type = BUILD_APP,
    .name = "app_name",
    .objd = "obj",
    .srcf = D("src/*.c"),
    .cflgs = "-Wall -O2",
}};
```
### C++
```c
Prj p[] = {{
    .type = BUILD_APP,
    .name = "app_name",
    .objd = "obj",
    .srcf = D("src/*.cpp"),
    .cflgs = "-Wall -O2",
    .cc = "g++",
}};
```

## Minimum Possible Configuration
### C
```c
Prj p[] = {{
    .name = "app_name",
    .srcf = D("main.c"),
}};
```

## Tested platforms
- Windows
- Cygwin
- (...the rest is missing some implementations/verification...)

## General Notice
- Don't have any spaces in any of the files having any business with this build tool
- If you use subfolders in a string, always use `/` and not `\`
- Among others, `Prj::name` and `Prj::obj` can be a sequence of subfolders

## Planned
- support for c++
- maybe multithreading
- make all platforms be able to use this
