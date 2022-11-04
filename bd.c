/* MIT License

Copyright (c) 2022 rphii

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

/* start of os detection */
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    // define something for Windows (32-bit and 64-bit, this part is common)
    #define OS_STR "Windows"
    #define OS_DEF "OS_WIN"
    #define OS_WIN
    #define SLASH_STR   "\\"
    #define SLASH_CH    '\\'
    #ifdef _WIN64
    #include "windows.h"
    #include "fileapi.h"
    #include "errhandlingapi.h"
        // define something for Windows (64-bit only)
    #else
        // define something for Windows (32-bit only)
    #endif
#else
    /* common things by all others */
    #define SLASH_STR   "/"
    #define SLASH_CH    '/'
#endif
#if defined(OS_WIN)
#elif defined(__CYGWIN__)
    #define OS_STR "Cygwin"
    #define OS_DEF "OS_CYGWIN"
    #define OS_CYGWIN
#elif __APPLE__
    #define OS_STR "Apple"
    #define OS_DEF "OS_APPLE"
    #define OS_APPLE
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
         // iOS, tvOS, or watchOS Simulator
    #elif TARGET_OS_MACCATALYST
         // Mac's Catalyst (ports iOS API into Mac, like UIKit).
    #elif TARGET_OS_IPHONE
        // iOS, tvOS, or watchOS device
    #elif TARGET_OS_MAC
        // Other kinds of Apple platforms
    #else
    #   error "Unknown Apple platform"
    #endif
#elif __ANDROID__
    #define OS_STR "Android"
    #define OS_DEF "OS_ANDROID"
    #define OS_ANDROID
    // Below __linux__ check should be enough to handle Android,
    // but something may be unique to Android.
#elif __linux__
    #define OS_STR "Linux"
    #define OS_DEF "OS_LINUX"
    #define OS_LINUX
    // linux
#elif __unix__ // all unices not caught above
    #define OS_STR "Unix"
    #define OS_DEF "OS_UNIX"
    #define OS_UNIX
    // Unix
#elif defined(_POSIX_VERSION)
    #define OS_STR "Posix"
    #define OS_DEF "OS_POSIX"
    #define OS_POSIX
    // POSIX
#else
#   error "Unknown compiler"
#endif
/* end of os detection */

/* start of globbing pattern */
#if defined(OS_WIN)
    #define FIND(p) "cmd /V /C \"@echo off && setlocal enabledelayedexpansion && set \"var= %s\" && set \"var=!var:/=\\!\" && for %%I in (!var!) do set \"file=%%~dpnxI\" && set \"file=!file:%%cd%%\\=!\" && @echo !file!\"", p
#elif defined(OS_CYGWIN)
    #define FIND(p) "find $(dirname \"%s\") -maxdepth 1 -type f -name \"$(basename \"%s\")\"", p, p
#endif
/* end of globbing pattern */

#define D(...)          (StrArr){.s = (char *[]){__VA_ARGS__}, .n = sizeof((char *[]){__VA_ARGS__})/sizeof(*(char *[]){__VA_ARGS__})}
#define BD_ERR(bd,retval,...)  do { if(!bd->noerr) { printf("\033[91m[ERROR:%d]\033[0m ", __LINE__); printf(__VA_ARGS__); printf("\n"); } bd->error = __LINE__; return retval; } while(0)
#define BD_MSG(bd,...)  if(!bd->quiet) { printf(__VA_ARGS__); printf("\n"); }
#define SIZE_ARRAY(x)   (sizeof(x)/sizeof(*x))

/* structs */

typedef struct StrArr {
    char **s;
    int n;
} StrArr;

typedef enum {
   CMD_BUILD,
   CMD_CLEAN,
   CMD_LIST,
   CMD_CONFIG,
   CMD_OS,
   CMD_HELP,
   CMD_QUIET,
   CMD_NOERR,
   // commands above
   CMD__COUNT
} CmdList;

static const char *cmds[CMD__COUNT] = {
   "build",
   "clean",
   "list",
   "conf",
   "os",
   "-h",
   "-q",
   "-e",
};

static const char *cmdsinfo[CMD__COUNT] = {
    "Build the projects",
    "Clean created files",
    "List all projects (simple view)",
    "List all configurations",
    "Print the Operating System",
    "Help output (this here)",
    "Execute quietly",
    "Also makes errors quiet",
};

typedef struct {
    StrArr ofiles;
    int cutoff;
    int error;
    int count;
    bool quiet;
    bool noerr;
    bool done;
} Bd;

typedef enum {
    BUILD_APP,
    BUILD_EXAMPLES,
    BUILD_STATIC,
    BUILD_SHARED,
    BUILD__COUNT,
} BuildList;
static const char *static_build_str[BUILD__COUNT] = {
    "App",
    "Example",
    "Static",
    "Shared",
};
static char *static_ext[BUILD__COUNT] = {
#if defined(OS_WIN)
    ".exe",
    ".exe",
    ".a",
    ".dll",
#elif defined(OS_CYGWIN)
    ".exe",
    ".exe",
    ".a",
    ".dll",
#endif
};
typedef struct Prj {
    char *cflgs;    /* compile flags / options */   
    char *lopts;    /* linker flags / options */
    char *llibs;    /* linker library */
    char *name;     /* name of the thing */
    char *objd;     /* object directory */
    StrArr srcf;    /* source files */
    BuildList type; /* type */
} Prj;

/* all function prototypes */
static char *strprf(char *format, ...);
static char *static_cc(BuildList type, char *flags, char *ofile, char *cfile);
static char *static_ld(BuildList type, char *options, char *name, char *ofiles, char *libstuff);
static void prj_print(Bd *bd, Prj *p, bool simple);
static StrArr *strarr_new();
static void strarr_free(StrArr *arr);
static bool strarr_set_n(StrArr *arr, int n);
static int str_append(char **str, char *format, ...);
static StrArr *parse_pipe(Bd *bd, char *cmd);
static StrArr *parse_dfile(Bd *bd, char *dfile);
static int strrstrn(const char *s1, const char *s2);
static uint64_t modtime(Bd *bd, const char *filename);
static uint64_t modlibs(Bd *bd, char *llibs);
static void makedir(const char *dirname);
static void compile(Bd *bd, Prj *p, char *name, char *objf, char *srcf);
static void link(Bd *bd, Prj *p, char *name);
static void build(Bd *bd, Prj *p);
static void clean(Bd *bd, Prj *p);
static void bd_execute(Bd *bd, CmdList cmd);

/* function implementations */
static char *strprf(char *format, ...)
{
    if(!format) return 0;
    /* calculate length of append string */
    va_list argp;
    va_start(argp, format);
    size_t len_app = vsnprintf(0, 0, format, argp);
    va_end(argp);
    /* make sure to have enough memory */
    char *result = malloc(len_app + 1);
    if(!result) return 0;
    /* actual append */
    va_start(argp, format);
    vsnprintf(result, len_app + 1, format, argp);
    va_end(argp);
    return result;
}

static char *static_cc(BuildList type, char *flags, char *ofile, char *cfile)
{
    switch(type) {
        case BUILD_APP      : ;
        case BUILD_EXAMPLES : return strprf("gcc -c -MMD -MP %s%s-D%s -o %s %s", flags ? flags : "", flags ? " " : "", OS_DEF, ofile, cfile);
        case BUILD_STATIC   : return strprf("gcc -c -MMD -MP %s%s-D%s -o %s %s", flags ? flags : "", flags ? " " : "", OS_DEF, ofile, cfile);
        case BUILD_SHARED   : return strprf("gcc -c -MMD -MP -fPIC -D%s%s%s -o %s %s", flags ? flags : "", flags ? " " : "", OS_DEF, ofile, cfile);
        default             : return 0;
    }
}
static char *static_ld(BuildList type, char *options, char *name, char *ofiles, char *libstuff)
{
    switch(type) {
        case BUILD_APP      : ;
        case BUILD_EXAMPLES : return strprf("gcc %s%s-o %s %s %s", options ? options : "", options ? " " : "", name, ofiles, libstuff ? libstuff : "");
        case BUILD_STATIC   : return strprf("ar rcs %s%s %s", name, static_ext[type], ofiles);
        case BUILD_SHARED   : return strprf("gcc -shared -fPIC %s%s-o %s%s %s %s", options ? options : "", options ? " " : "", name, static_ext[type], ofiles, libstuff ? libstuff : "");
        default             : return 0;
    }
}

static void prj_print(Bd *bd, Prj *p, bool simple) /* TODO list if they're up to date, & clean up the mess with EXAMPLES (difference in listing/cleaning/building...) */
{
    if(!p) return;
    if(p->type != BUILD_EXAMPLES) {
        printf("%-7s : [%s]\n", static_build_str[p->type], p->name);
    } else {
        for(int k = 0; k < p->srcf.n; k++) {
            char *cmd = strprf(FIND(p->srcf.s[k]));
            StrArr *res = parse_pipe(bd, cmd);
            if(!res) BD_ERR(bd,, "Pipe returned nothing");
            for(int i = 0; i < res->n; i++) {
                int ext = strrstrn(res->s[i], ".c");
                int dir = strrstrn(res->s[i], SLASH_STR);
                char *name = strprf("%.*s", ext - dir - 1, &res->s[i][dir + 1]);
                printf("%-7s : [%s]\n", static_build_str[p->type], name);
                free(name);
            }
            free(res);
            free(cmd);
        }
    }
    if(simple) return;
    printf("  cflgs = %s\n", p->cflgs);
    printf("  lopts = %s\n", p->lopts);
    printf("  llibs = %s\n", p->llibs);
    printf("  [%s] : \n", p->objd);
    for(int i = 0; i < p->srcf.n; i++) printf("%4s%s\n", "", p->srcf.s[i]);
}

static StrArr *strarr_new()
{
    StrArr *result = malloc(sizeof(*result));
    if(!result) return 0;
    memset(result, 0, sizeof(*result));
    return result;
}

static void strarr_free(StrArr *arr)
{
    if(!arr) return;
    for(int i = 0; i < arr->n; i++) free(arr->s[i]);
    free(arr->s);
    arr->s = 0;
    arr->n = 0;
}

static bool strarr_set_n(StrArr *arr, int n)
{
    if(!arr || !n) return false;
    if(arr->n == n) return true;
    void *temp = realloc(arr->s, sizeof(*arr->s) * (n));
    if(!temp) return false;
    arr->s = temp;
    memset(&arr->s[arr->n], 0, sizeof(*arr->s) * (n - arr->n));
    arr->n = n;
    return true;
}

static int str_append(char **str, char *format, ...)
{
    if(!str || !format) return 0;
    /* calculate length of append string */
    va_list argp;
    va_start(argp, format);
    size_t len_app = vsnprintf(0, 0, format, argp);
    va_end(argp);
    size_t len_is = *str ? strlen(*str) : 0;
    /* make sure to have enough memory */
    size_t len_allocd = (len_is / 128) * 128;
    size_t len_reqd = ((len_is + len_app + 1) / 128 + 1) * 128;
    if(len_reqd > len_allocd) {
        void *temp = realloc(*str, len_is + len_app + 1);
        if(!temp) return 0;
        *str = temp;
    }
    /* actual append */
    va_start(argp, format);
    size_t len_p = vsnprintf(&((*str)[len_is]), len_app + 1, format, argp);
    va_end(argp);
    (*str)[len_is + len_p] = 0;
    return len_is + len_p;
}

static StrArr *parse_pipe(Bd *bd, char *cmd)
{
    FILE *fp = popen(cmd, "rb");
    if(!fp) BD_ERR(bd, 0, "Could not open pipe");

    // printf("[[%s]]\n", cmd);
    StrArr *result = strarr_new();

    int c = fgetc(fp);
    int n = 1;
    while(c != EOF) {
        if(c == '\n') n++;
        else if(c != '\r') {
            if(!strarr_set_n(result, n)) BD_ERR(bd, 0, "Failed to modify StrArr");
            str_append(&result->s[result->n - 1], "%c", c);
        }
        c = fgetc(fp);
    }

    if(pclose(fp)) BD_ERR(bd, 0, "Could not close pipe");
    return result;
}

static StrArr *parse_dfile(Bd *bd, char *dfile)
{
    FILE *fp = fopen(dfile, "rb");
    if(!fp) return 0;

    StrArr *result = strarr_new();

    /* skip first line */
    int c = fgetc(fp);
    while(c != '\n') {
        if(c == '\\') {
            c = fgetc(fp);  /* skip '\r' under windows ; '\n' under linux */
            if(c == '\r') fgetc(fp);  /* skip '\n' under windows */
        }    
        c = fgetc(fp);
    }
    c = fgetc(fp);
    /* read in the required files */
    int n = 1;
    while(c != EOF) {
        if(c == '\n') n++;
        else if(c == ':') fgetc(fp);
        else {
            if(!strarr_set_n(result, n)) BD_ERR(bd, 0, "Failed to modify StrArr");
            str_append(&result->s[result->n - 1], "%c", c);
        }
        c = fgetc(fp);
    }

    if(fclose(fp)) BD_ERR(bd, 0, "Could not close dfile");
    return result;
}

static int strrstrn(const char *s1, const char *s2)
{
   const int len_s1 = strlen(s1);
   const int len_s2 = strlen(s2);
   for(int i = len_s1 - len_s2; i > 0; i--) {
      if(!strncmp(&s1[i], s2, len_s2)) return i;
   }
   return -1;
}

static uint64_t modtime(Bd *bd, const char *filename)
{
#if defined(OS_WIN)
    HANDLE filehandle = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(filehandle == INVALID_HANDLE_VALUE) {
        DWORD lasterr = GetLastError();
        if(lasterr != ERROR_FILE_NOT_FOUND && lasterr != ERROR_PATH_NOT_FOUND) {
            BD_ERR(bd, 0, "%s: Failed to get handle (code %ld)", filename, lasterr);
        }
        return 0;
    }
    FILETIME t;
    BOOL gottime = GetFileTime(filehandle, 0, 0, &t);
    if(!gottime) {
        BD_ERR(bd, 0, "%s: Failed to retrieve time information (code %ld)", filename, GetLastError());
    }
    CloseHandle(filehandle);
    ULARGE_INTEGER result = {.HighPart = t.dwHighDateTime, .LowPart = t.dwLowDateTime};
    return result.QuadPart;
#elif defined(OS_CYGWIN)
    struct stat attr;
    if(stat(filename, &attr) == -1) return 0;
    return (uint64_t)attr.st_ctime;
#endif
}

/* return the most recend library time */
static uint64_t modlibs(Bd *bd, char *llibs)
{
    if(!llibs) return 0;
    int llibs_len = strlen(llibs);
    /* filter out paths (-L= flag) */
    char *lpath = llibs;
    StrArr *lpaths = strarr_new();
    while(*lpath) {
        lpath = strstr(lpath, "-L=");
        if(!lpath) break;
        lpath += 3; /* "-L=" */
        char *space = memchr(lpath, ' ', llibs + llibs_len - lpath);
        space = space ? space : lpath + llibs_len;
        if(!strarr_set_n(lpaths, lpaths->n + 1)) BD_ERR(bd, 0, "Failed to modify StrArr");
        lpaths->s[lpaths->n - 1] = strprf("%.*s", (int)(space - lpath), lpath);
        lpath = space + 1;
    }
    /* filter out names (-l= flag)*/
    char *lname = llibs;
    StrArr *lnames = strarr_new();
    while(*lname) {
        lname = strstr(lname, "-l=");
        if(!lname) break;
        lname += 3; /* "-l= "*/
        char *space = memchr(lname, ' ', llibs + llibs_len - lname);
        space = space ? space : lpath + llibs_len;
        if(!strarr_set_n(lnames, lnames->n + 1)) BD_ERR(bd, 0, "Failed to modify StrArr");
        lnames->s[lnames->n - 1] = strprf("%.*s", (int)(space - lname), lname);
        lpath = space + 1;
    }
    /* check if library changed */
    uint64_t recent = 0;
    for(int i = 0; i < lpaths->n; i++) {
        for(int j = 0; j < lnames->n; j++) {
            char *libstatic = strprf("%s%slib%s%s", lpaths->s[i], SLASH_STR, lnames->s[j], static_ext[BUILD_STATIC]);
            char *libshared = strprf("%s%slib%s%s", lpaths->s[i], SLASH_STR, lnames->s[j], static_ext[BUILD_SHARED]);
            uint64_t modstatic = modtime(bd, libstatic);
            uint64_t modshared = modtime(bd, libshared);
            recent = modstatic > recent ? modstatic : recent;
            recent = modshared > recent ? modshared : recent;
        }
    }

    free(lpaths);
    free(lnames);
    return recent;
}

static void makedir(const char *dirname)
{
#if defined(OS_WIN)
   // TODO make quiet (somehow it is?! or isn't?! idk)
   mkdir(dirname);
#elif defined(OS_CYGWIN)
   mkdir(dirname, 0700);
#endif
}

static void compile(Bd *bd, Prj *p, char *name, char *objf, char *srcf)
{
    char *cc = static_cc(p->type, p->cflgs, objf, srcf);
    if(!strarr_set_n(&bd->ofiles, bd->ofiles.n + 1)) BD_ERR(bd,, "Failed to modify StrArr");
    str_append(&bd->ofiles.s[bd->ofiles.n - 1], objf);
    BD_MSG(bd, "[\033[96m%s\033[0m] %s", name, cc); /* bright cyan color */
    bd->error = system(cc);
    free(cc);
}

static void link(Bd *bd, Prj *p, char *name)
{
    if(bd->error) return;
    if(bd->ofiles.n) {
        /* link */
        char *ofiles = 0;
        for(int i = 0; i < bd->ofiles.n; i++) str_append(&ofiles, "%s%s", bd->ofiles.s[i], i + 1 < bd->ofiles.n ? " " : "");
        char *ld = static_ld(p->type, p->lopts, name, ofiles, p->llibs);
        BD_MSG(bd, "[\033[93m%s\033[0m] %s", name, ld); /* bright yellow color*/
        bd->error = system(ld);
        free(ofiles);
        free(ld);
        strarr_free(&bd->ofiles);
    } else {
        BD_MSG(bd, "[\033[92m%s\033[0m] is up to date", name); /* bright green color */
    }
}

static void build(Bd *bd, Prj *p)
{
    if(!bd) return;
    if(bd->error) return;
    
    char *appstr = strprf("%s%s", p->name, static_ext[p->type]);
    uint64_t moda = modtime(bd, appstr);
    free(appstr);
    bool newbuild = (moda == 0);
    /* check if any dependant library was modified */
    uint64_t modl = modlibs(bd, p->llibs);
    bool newlink = (modl > moda);

    makedir(p->objd);
    for(int k = 0; k < p->srcf.n && !bd->error; k++) {
        char *cmd = strprf(FIND(p->srcf.s[k]));
        StrArr *res = parse_pipe(bd, cmd);
        if(!res) BD_ERR(bd,, "Pipe returned nothing");
        for(int i = 0; i < res->n; i++) {
            int ext = strrstrn(res->s[i], ".c");
            int dir = strrstrn(res->s[i], SLASH_STR);
            char *name = 0;
            if(p->type != BUILD_EXAMPLES) {
                name = p->name;
            } else {
                name = strprf("%.*s", ext - dir - 1, &res->s[i][dir + 1]); /* TODO dangerous ?! */
                appstr = strprf("%s%s", name, static_ext[p->type]);
                moda = modtime(bd, appstr);
                newbuild = (moda == 0);
                newlink = (modl > moda);
                free(appstr);
            }
            char *srcf = res->s[i];
            char *objf = strprf("%s%.*s.o", p->objd, ext - dir, &res->s[i][dir]);
            char *dfile = strprf("%s%.*s.d", p->objd, ext - dir, &res->s[i][dir]);
            uint64_t modc = modtime(bd, srcf);
            uint64_t modo = modtime(bd, objf);
            if(!newbuild && modo >= modc) {
                /* check .d file in objd */
                StrArr *hfiles = parse_dfile(bd, dfile);
                for(int j = 0; hfiles && j < hfiles->n; j++) {
                    // printf("[DEP] %s\n", hfiles->s[j]);
                    uint64_t modh = modtime(bd, hfiles->s[j]);
                    if(modh > modo) {
                        compile(bd, p, name, objf, srcf);
                        break;
                    }
                    else if(newlink) { /* TODO IMPORTANT is this condition really, REALLY correct????... _maybe_ it is now....? */
                        if(!strarr_set_n(&bd->ofiles, bd->ofiles.n + 1)) BD_ERR(bd,, "Failed to modify StrArr");
                        bd->ofiles.s[bd->ofiles.n - 1] = strprf("%s", objf);
                        break;
                    }
                }
                strarr_free(hfiles);
                free(hfiles);
                
            } else {
                compile(bd, p, name, objf, srcf);
            }


            if(p->type == BUILD_EXAMPLES) {
                link(bd, p, name);
                free(name);
            }

            free(objf);
            free(dfile);
        }
        strarr_free(res);
        free(res);
        free(cmd);
    }
    if(p->type != BUILD_EXAMPLES) link(bd, p, p->name);
}

static void clean(Bd *bd, Prj *p)
{
    char *exes = 0;
    if(p->type != BUILD_EXAMPLES) {
        exes = strprf("%s%s", p->name, static_ext[p->type]);
    } else {
        for(int k = 0; k < p->srcf.n && !bd->error; k++) {
            char *cmd = strprf(FIND(p->srcf.s[k]));
            StrArr *res = parse_pipe(bd, cmd);
            if(!res) BD_ERR(bd,, "Pipe returned nothing");
            for(int i = 0; i < res->n; i++) {
                int ext = strrstrn(res->s[i], ".c");
                int dir = strrstrn(res->s[i], SLASH_STR);
                char *name = strprf("%.*s", ext - dir - 1, &res->s[i][dir + 1]);
                char *appstr = strprf("%s%s ", name, static_ext[p->type]);
                str_append(&exes, "%s", appstr);
                free(name);
                free(appstr);
            }
            strarr_free(res);
            free(res);
            free(cmd);
        }
    }
#if defined(OS_WIN)
    char *delfiles = strprf("del /q %s %s 2>nul", p->objd, exes);
    char *delfolder = strprf("rmdir %s 2>nul", p->objd);
    BD_MSG(bd, "[\033[95m%s\033[0m] %s", exes, delfiles); /* bright magenta */
    system(delfiles);
    BD_MSG(bd, "[\033[95m%s\033[0m] %s", exes, delfolder); /* bright magenta */
    system(delfolder);
    free(delfiles);
    free(delfolder);
#elif defined(OS_CYGWIN)
    char *del = strprf("rm -rf %s %s", p->objd, exes);
    BD_MSG(bd, "[\033[95m%s\033[0m] %s", exes, del); /* bright magenta */
    system(del);
    free(del);
#endif
    free(exes);
}

static void bd_execute(Bd *bd, CmdList cmd)
{
    Prj p[] = {{
        .type = BUILD_APP,
        .name = "a",
        .objd = "obj",
        .srcf = D("src/*.c"),
        .cflgs = "-Wall -O2",
    }};

    switch(cmd) {
        case CMD_BUILD: {
            for(int i = 0; i < SIZE_ARRAY(p); i++) build(bd, &p[i]);
            bd->done = true;
        } break;
        case CMD_CLEAN: {
            for(int i = 0; i < SIZE_ARRAY(p); i++) clean(bd, &p[i]);
            bd->done = true;
        } break;
        case CMD_LIST: {
            for(int i = 0; i < SIZE_ARRAY(p); i++) prj_print(bd, &p[i], true);
            bd->done = true;
        } break;
        case CMD_CONFIG: {
            for(int i = 0; i < SIZE_ARRAY(p); i++) prj_print(bd, &p[i], false);
            bd->done = true;
        } break;
        case CMD_OS: {
            printf(OS_STR"\n");
            bd->done = true;
        } break;
        case CMD_HELP: {
            for(int i = 0; i < CMD__COUNT; i++) printf("%2s%-8s%s\n", "", cmds[i], cmdsinfo[i]);
            bd->done = true;
        } break;
        case CMD_QUIET: {
            bd->quiet = true;
        } break;
        case CMD_NOERR: {
            bd->noerr = true;
        } break;
        default: break;
    }
}

/* TODO create any missing paths on the fly */

/* start of program */
int main(int argc, const char **argv)
{
    Bd bd = {0};
    /* create base directory */
    bd.cutoff = strrstrn(argv[0], SLASH_STR);
    bd.cutoff = bd.cutoff ? bd.cutoff + 1 : 0;
    /* go over command line args */
    for(int i = 1; i < argc; i++) {
        for(CmdList j = 0; j < CMD__COUNT; j++) {
            if(strcmp(argv[i], cmds[j])) continue;
            bd_execute(&bd, j);
        }
    }
    if(!bd.done) bd_execute(&bd, CMD_BUILD);
    return bd.error;
}
