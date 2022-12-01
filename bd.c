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
    /* define something for Windows (32-bit and 64-bit, this part is common) */
    #define OS_STR "Windows"
    #define OS_DEF "OS_WIN"
    #define OS_WIN
    #define SLASH_STR   "\\"
    #include "windows.h"
    #include "fileapi.h"
    #include "errhandlingapi.h"
    #ifdef _WIN64
        /* define something for Windows (64-bit only) */
    #else
        /* define something for Windows (32-bit only) */
    #endif
#else
    /* common things by all others */
    #define SLASH_STR   "/"
    #include <errno.h>
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
         /* iOS, tvOS, or watchOS Simulator */
    #elif TARGET_OS_MACCATALYST
         /* Mac's Catalyst (ports iOS API into Mac, like UIKit). */
    #elif TARGET_OS_IPHONE
        /* iOS, tvOS, or watchOS device */
    #elif TARGET_OS_MAC
        /* Other kinds of Apple platforms */
    #else
    #error "Unknown Apple platform"
    #endif
#elif __ANDROID__
    #define OS_STR "Android"
    #define OS_DEF "OS_ANDROID"
    #define OS_ANDROID
    /* Below __linux__ check should be enough to handle Android, */
    /* but something may be unique to Android. */
#elif __linux__
    #define OS_STR "Linux"
    #define OS_DEF "OS_LINUX"
    #define OS_LINUX
    /* linux */
#elif __unix__ /* all unices not caught above */
    #define OS_STR "Unix"
    #define OS_DEF "OS_UNIX"
    #define OS_UNIX
    /* Unix */
#elif defined(_POSIX_VERSION)
    #define OS_STR "Posix"
    #define OS_DEF "OS_POSIX"
    #define OS_POSIX
    /* POSIX */
#else
#error "Unknown compiler"
#endif
/* end of os detection */

/* start of globbing pattern */
#if defined(OS_WIN)
    #define FIND(p) "cmd /V /C \"@echo off && setlocal enabledelayedexpansion && set \"var= %s\" && set \"var=!var:/=\\!\" && for %%I in (!var!) do set \"file=%%~dpnxI\" && set \"file=!file:%%cd%%\\=!\" && @echo !file!\"", p
#elif defined(OS_CYGWIN) || defined(OS_APPLE) || defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_POSIX)
    #define FIND(p) "find \"$(dirname \"%s\")\" -maxdepth 1 -type f -name \"$(basename \"%s\")\"", p, p
#endif
/* end of globbing pattern */

#ifndef CONFIG
#define CONFIG  "bd.conf"
#endif

#define D(...)          (StrArr){.s = (char *[]){__VA_ARGS__}, .n = sizeof((char *[]){__VA_ARGS__})/sizeof(*(char *[]){__VA_ARGS__})}
#define BD_ERR(bd,retval,...)  do { if(!bd->noerr) { printf("\033[91;1m[ERROR:%s:%d]\033[0m ", __func__, __LINE__); printf(__VA_ARGS__); printf("\n"); } bd->error = __LINE__; return retval; } while(0)
#define BD_MSG(bd,...)  if(!bd->quiet) { printf(__VA_ARGS__); printf("\n"); }
#define BD_VERBOSE(bd,mes,...)  if(bd->verbose) { printf("%d:%s - "mes"\n", __LINE__, __func__, ##__VA_ARGS__); }
#define SIZE_ARRAY(x)   (sizeof(x)/sizeof(*x))
#define strarr_free_p(x)    do { strarr_free(x); free(x); x = 0; } while(0)
#define strarr_free_pa(...) do { for(int i = 0; i < SIZE_ARRAY(((StrArr*[]){__VA_ARGS__})); i++) strarr_free_p(((StrArr*[]){__VA_ARGS__})[i]); } while(0)

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
   CMD_VERBOSE,
   /* commands above */
   CMD__COUNT
} CmdList;
static const char *static_cmds[CMD__COUNT] = {
   "build",
   "clean",
   "list",
   "conf",
   "os",
   "-h",
   "-q",
   "-e",
   "-v",
};
static const char *static_cmdsinfo[CMD__COUNT] = {
    "Build the projects",
    "Clean created files",
    "List all projects (simple view)",
    "List all configurations",
    "Print the Operating System",
    "Help output (this here)",
    "Execute quietly",
    "Also makes errors quiet",
    "Verbose output",
};

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
#if defined(OS_WIN) || defined(OS_CYGWIN)
    ".exe",
    ".exe",
    ".a",
    ".dll",
#elif defined(OS_APPLE) || defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_POSIX)
    "",
    "",
    ".a",
    ".so",
#endif
};

typedef struct Bd {
    StrArr ofiles;
    int error;
    int count;
    bool quiet;
    bool noerr;
    bool done;
    bool verbose;
    char *cc_cxx;
    bool use_cxx;
} Bd;

typedef struct Prj {
    char *cc;       /* c compiler */
    char *cxx;      /* cpp compiler */
    char *cflgs;    /* compile flags / options */   
    char *lopts;    /* linker flags / options */
    char *llibs;    /* linker library */
    char *name;     /* name of the thing */
    char *objd;     /* object directory */
    StrArr srcf;    /* source files */
    BuildList type; /* type */
} Prj;

static char static_cc_def[] = "gcc";
static char static_cxx_def[] = "g++";

/* all function prototypes */
static char *strprf(char *str, char *format, ...);
static char *static_cc_cxx(Bd *bd, Prj *p, char *ofile, char *cfile);
static char *static_ld(Bd *bd, Prj *p, char *name, char *ofiles, char *libstuff);
static void prj_print(Bd *bd, Prj *p, bool simple);
static StrArr *strarr_new();
static void strarr_free(StrArr *arr);
static bool strarr_set_n(StrArr *arr, int n);
static bool parse_pipe(Bd *bd, char *cmd, StrArr **result);
static StrArr *parse_dfile(Bd *bd, char *dfile);
static int strrstr(const char *s1, const char *s2);
static uint64_t modtime(Bd *bd, const char *filename);
static uint64_t modlibs(Bd *bd, char *llibs);
static void makedir(const char *dirname);
static StrArr *extract_dirs(Bd *bd, char *path, bool skiplast);
static void compile(Bd *bd, Prj *p, char *name, char *objf, char *srcf);
static void link(Bd *bd, Prj *p, char *name, bool avoidlink);
static void build(Bd *bd, Prj *p);
static void clean(Bd *bd, Prj *p);
static void bd_execute(Bd *bd, CmdList cmd);
static StrArr *prj_names(Bd *bd, Prj *p, StrArr *srcfs);
static StrArr *prj_srcfs(Bd *bd, Prj *p);
static StrArr *prj_srcfs_chg_dirext(Bd *bd, StrArr *srcfs, char *new_dir, char *new_ext);

/* function implementations */
static char *strprf(char *str, char *format, ...)
{
    if(!format) return 0;
    /* calculate length of append string */
    va_list argp;
    va_start(argp, format);
    int len_app = vsnprintf(0, 0, format, argp);
    int len_original = 0;
    va_end(argp);
    /* make sure to have enough memory */
    char *original = 0;
    if(str) {
        len_original = strlen(str);
        original = strprf(0, str);
    }
    void *temp = realloc(str, len_original + len_app + 1);
    if(!temp) return 0;
    char *result = temp;
    if(result != str) {
        memcpy(result, original, len_original);
    }
    /* actual append */
    va_start(argp, format);
    vsnprintf(&result[len_original], len_app + 1, format, argp);
    va_end(argp);
    result[len_original + len_app] = 0;
    return result;
}

static char *static_cc_cxx(Bd *bd, Prj *p, char *ofile, char *cfile)
{
    char *cc_use = 0;
    if(strrstr(cfile, ".c") == strlen(cfile) - 2) {
        cc_use = p->cc ? p->cc : static_cc_def;
        if(!bd->use_cxx) bd->cc_cxx = cc_use;
    }
    if(strrstr(cfile, ".cc") == strlen(cfile) - 3 || strrstr(cfile, ".cpp") == strlen(cfile) - 4) {
        cc_use = p->cxx ? p->cxx : static_cxx_def;
        bd->cc_cxx = cc_use;
        bd->use_cxx = true;
    }
    if(!cc_use) BD_ERR(bd, 0, "Unsupported file extension");
    switch(p->type) {
        case BUILD_APP      : ;
        case BUILD_EXAMPLES : return strprf(0, "%s -c -MMD -MP %s%s-D%s -o %s %s", cc_use, p->cflgs ? p->cflgs : "", p->cflgs ? " " : "", OS_DEF, ofile, cfile);
        case BUILD_STATIC   : return strprf(0, "%s -c -MMD -MP %s%s-D%s -o %s %s", cc_use, p->cflgs ? p->cflgs : "", p->cflgs ? " " : "", OS_DEF, ofile, cfile);
        case BUILD_SHARED   : return strprf(0, "%s -c -MMD -MP -fPIC -D%s%s%s -o %s %s", cc_use, p->cflgs ? p->cflgs : "", p->cflgs ? " " : "", OS_DEF, ofile, cfile);
        default             : return 0;
    }
}
static char *static_ld(Bd *bd, Prj *p, char *name, char *ofiles, char *libstuff)
{
    switch(p->type) {
        case BUILD_APP      : ;
        case BUILD_EXAMPLES : return strprf(0, "%s %s%s-o %s %s %s", bd->cc_cxx, p->lopts ? p->lopts : "", p->lopts ? " " : "", name, ofiles, libstuff ? libstuff : "");
        case BUILD_STATIC   : return strprf(0, "ar rcs %s%s %s", name, static_ext[p->type], ofiles);
        case BUILD_SHARED   : return strprf(0, "%s -shared -fPIC %s%s-o %s%s %s %s", bd->cc_cxx, p->lopts ? p->lopts : "", p->lopts ? " " : "", name, static_ext[p->type], ofiles, libstuff ? libstuff : "");
        default             : return 0;
    }
}

static void prj_print(Bd *bd, Prj *p, bool simple) /* TODO list if they're up to date */
{
    if(!p) return;
    /* gather files */
    StrArr *srcfs = prj_srcfs(bd, p);
    if(!srcfs) BD_ERR(bd,, "No source files");
    StrArr *objfs = prj_srcfs_chg_dirext(bd, srcfs, p->objd, ".o");
    if(!objfs) BD_ERR(bd,, "No object files");
    StrArr *depfs = prj_srcfs_chg_dirext(bd, srcfs, p->objd, ".d");
    if(!depfs) BD_ERR(bd,, "No dependency files");
    StrArr *targets = prj_names(bd, p, srcfs);
    if(!targets) BD_ERR(bd,, "No targets");

    /* print all names */
    for(int i = 0; i < targets->n; i++) printf("%-7s : \033[1m[ %s ]\033[0m\n", static_build_str[p->type], targets->s[i]);
    strarr_free_pa(srcfs, targets);
    if(simple) return;
    /* print the configuration */
    printf("  cc    = %s\n", p->cc ? p->cc : static_cc_def);
    printf("  cxx   = %s\n", p->cxx ? p->cxx : static_cxx_def);
    if(p->cflgs) printf("  cflgs = %s\n", p->cflgs);
    if(p->lopts) printf("  lopts = %s\n", p->lopts);
    if(p->llibs) printf("  llibs = %s\n", p->llibs);
    if(p->objd) printf("  objd  = [%s]\n", p->objd);
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

static bool parse_pipe(Bd *bd, char *cmd, StrArr **result)
{
    FILE *fp = popen(cmd, "r");
    if(!fp) BD_ERR(bd, false, "Could not open pipe");

    if(!*result) *result = strarr_new();
    if(!*result) BD_ERR(bd, false, "Failed to create StrArr");
    int c = fgetc(fp);
    int n = (*result)->n + 1;
    while(c != EOF) {
        if(c == '\n') n++;
        else if(c != '\r') {
            if(!strarr_set_n(*result, n)) BD_ERR(bd, false, "Failed to modify StrArr");
            (*result)->s[(*result)->n - 1] = strprf((*result)->s[(*result)->n - 1], "%c", c);
        }
        c = fgetc(fp);
    }

    if(pclose(fp)) BD_ERR(bd, false, "Could not close pipe");
    if(!(*result)->n) {    /* TODO check other functions that use `strarr_new()` to maybe also return 0 when empty... */
        free(*result);
        *result = 0;
    }
    return true;
}

static StrArr *parse_dfile(Bd *bd, char *dfile)
{
    FILE *fp = fopen(dfile, "rb");
    if(!fp) return 0;

    StrArr *result = strarr_new();
    if(!result) BD_ERR(bd, 0, "Failed to create StrArr");

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
    int n = 0;
    while(c != EOF) {
        if(c == '\n') n++;
        else if(c == ':' || c == '\r') {}
        else {
            if(!n) n = 1;
            if(!strarr_set_n(result, n)) BD_ERR(bd, 0, "Failed to modify StrArr");
            result->s[result->n - 1] = strprf(result->s[result->n - 1], "%c", c);
        }
        c = fgetc(fp);
    }

    BD_VERBOSE(bd, "found %d header files for '%s'", result->n, dfile);
    if(!result->n) strarr_free_p(result);

    if(fclose(fp)) BD_ERR(bd, 0, "Could not close dfile");
    return result;
}

static int strrstr(const char *s1, const char *s2)
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
#elif defined(OS_CYGWIN) || defined(OS_APPLE) || defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_POSIX)
    struct stat attr = {0};
    if(stat(filename, &attr) == -1 && errno != ENOENT ) {
        BD_ERR(bd, 0, "%s: %s", filename, strerror(errno));
    }
    return (uint64_t)attr.st_ctime;
#endif
}

/* return the most recend library time */
static uint64_t modlibs(Bd *bd, char *llibs)
{
    if(!llibs) return 0;
    int llibs_len = strlen(llibs);
    /* extract paths / names from llibs */
    char *find[] = {"-L", "-l"};
    StrArr *arr_Ll[] = {strarr_new(), strarr_new()};
    for(int i = 0; i < (int)SIZE_ARRAY(arr_Ll); i++) {
        if(!arr_Ll[i]) BD_ERR(bd, 0, "Failed to create StrArr");
        char *search = llibs;
        while(*search) {
            search = strstr(search, find[i]);
            if(!search) break;
            search += strlen(find[i]) + 1;
            char *space = memchr(search, ' ', llibs + llibs_len - search);
            space = space ? space : search + llibs_len;
            if(!strarr_set_n(arr_Ll[i], arr_Ll[i]->n + 1)) BD_ERR(bd, 0, "Failed to modify StrArr");
            arr_Ll[i]->s[arr_Ll[i]->n - 1] = strprf(0, "%.*s", (int)(space - search), search);
            search = space + 1;
        }
    }
    /* finally get the most recent modified time */
    uint64_t recent = 0;
    for(int i = 0; i < arr_Ll[0]->n; i++) {
        for(int j = 0; j < arr_Ll[1]->n; j++) {
            char *libstatic = strprf(0, "%s%slib%s%s", arr_Ll[0]->s[i], SLASH_STR, arr_Ll[1]->s[j], static_ext[BUILD_STATIC]);
            char *libshared = strprf(0, "%s%slib%s%s", arr_Ll[0]->s[i], SLASH_STR, arr_Ll[1]->s[j], static_ext[BUILD_SHARED]);
            uint64_t modstatic = modtime(bd, libstatic);
            uint64_t modshared = modtime(bd, libshared);
            BD_VERBOSE(bd, "modified time of static library '%s' : %zu", libstatic, (size_t)modstatic);
            BD_VERBOSE(bd, "modified time of shared library '%s' : %zu", libshared, (size_t)modshared);
            recent = modstatic > recent ? modstatic : recent;
            recent = modshared > recent ? modshared : recent;
        }
    }
    /* free all used arrs */
    for(int i = 0; i < (int)SIZE_ARRAY(arr_Ll); i++) {
        strarr_free_p(arr_Ll[i]);
    }
    return recent;
}

static void makedir(const char *dirname)
{
#if defined(OS_WIN)
   /* TODO is this quiet?! if not make it quiet */
   mkdir(dirname);
#elif defined(OS_CYGWIN) || defined(OS_APPLE) || defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_POSIX)
   mkdir(dirname, 0700);
#endif
}

static StrArr *extract_dirs(Bd *bd, char *path, bool skiplast)
{
    StrArr *result = strarr_new();
    /* go over the given path and split by '/' */
    int len = path ? strlen(path) : 0;
    for(int i = 0; i < len; i++) {
        bool mkd = false;
        if(path[i] == '/') mkd = true;
        else if(i + 1 == len && !skiplast) mkd = true;
        if(mkd) {
            if(!strarr_set_n(result, result->n + 1)) BD_ERR(bd, 0, "Failed to modify StrArr");
            result->s[result->n - 1] = strprf(0, "%.*s", i + 1, path);
        }
    }
    return result;
}

static void compile(Bd *bd, Prj *p, char *name, char *objf, char *srcf)
{
    char *cc = static_cc_cxx(bd, p, objf, srcf);
    if(!strarr_set_n(&bd->ofiles, bd->ofiles.n + 1)) BD_ERR(bd,, "Failed to modify StrArr");
    bd->ofiles.s[bd->ofiles.n - 1] = strprf(bd->ofiles.s[bd->ofiles.n - 1], objf);
    BD_MSG(bd, "\033[94;1m[ %s ]\033[0m %s", name, cc); /* bright blue color */
    bd->error = system(cc);
    free(cc);
}

static void link(Bd *bd, Prj *p, char *name, bool avoidlink)
{
    if(bd->error) return;
    if(bd->ofiles.n && !avoidlink) {
        /* link */
        char *ofiles = 0;
        for(int i = 0; i < bd->ofiles.n; i++) ofiles = strprf(ofiles, "%s%s", bd->ofiles.s[i], i + 1 < bd->ofiles.n ? " " : "");
        char *ld = static_ld(bd, p, name, ofiles, p->llibs);
        BD_MSG(bd, "\033[93;1m[ %s ]\033[0m %s", name, ld); /* bright yellow color*/
        bd->error = system(ld);
        free(ofiles);
        free(ld);
    } else {
        BD_MSG(bd, "\033[92;1m[ %s ]\033[0m is up to date", name); /* bright green color */
    }
    bd->cc_cxx = static_cc_def;
    bd->use_cxx = false;
    strarr_free(&bd->ofiles);
}

static void build(Bd *bd, Prj *p)
{
    if(!bd) return;
    if(bd->error) return;
    /* get most recent modified time of any included library */
    uint64_t m_llibs = modlibs(bd, p->llibs);
    /* gather all files */
    StrArr *dirn = extract_dirs(bd, p->name, (bool)(p->type != BUILD_EXAMPLES));
    if(!dirn) BD_ERR(bd,, "Failed to get directories from name");
    BD_VERBOSE(bd, "extracted %d directories from '%s'", dirn->n, p->name);
    StrArr *diro = extract_dirs(bd, p->objd, false);
    if(!diro) BD_ERR(bd,, "Failed to get directories from objd");
    BD_VERBOSE(bd, "extracted %d directories from '%s'", dirn->n, p->objd);
    StrArr *srcfs = prj_srcfs(bd, p);
    if(!srcfs) BD_ERR(bd,, "No source files");
    BD_VERBOSE(bd, "extracted %d source files", srcfs->n);
    StrArr *objfs = prj_srcfs_chg_dirext(bd, srcfs, p->objd, ".o");
    if(!objfs) BD_ERR(bd,, "No object files");
    BD_VERBOSE(bd, "converted %d source files to object files", srcfs->n);
    StrArr *depfs = prj_srcfs_chg_dirext(bd, srcfs, p->objd, ".d");
    if(!depfs) BD_ERR(bd,, "No dependency files");
    BD_VERBOSE(bd, "converted %d source files to dependency files", srcfs->n);
    StrArr *targets = prj_names(bd, p, srcfs);
    if(!targets) BD_ERR(bd,, "No targets to build");
    bool relink = false;
    /* create folders */
    for(int i = 0; i < dirn->n; i++) makedir(dirn->s[i]);
    for(int i = 0; i < diro->n; i++) makedir(diro->s[i]);
    /* now compile it */
    for(int k = 0; k < targets->n && !bd->error; k++) {
        /* maybe check if target even exists */
        char *targetstr = strprf(0, "%s%s", targets->s[k], static_ext[p->type]);
        uint64_t m_target = modtime(bd, targetstr);
        BD_VERBOSE(bd, "modified time of target '%s' = %zu", targetstr, (size_t)m_target);
        bool newlink = (bool)(m_llibs > m_target) || (bool)(m_target == 0);
        free(targetstr);
        /* set up loop */
        int i0 = (p->type == BUILD_EXAMPLES) ? k : 0;
        int iE = (p->type == BUILD_EXAMPLES) ? k + 1 : srcfs->n;
        for(int i = i0; i < iE && !bd->error; i++) {
            /* go over source file(s) */
            uint64_t m_srcf = modtime(bd, srcfs->s[i]);
            BD_VERBOSE(bd, "modified time of source '%s' = %zu", srcfs->s[i], (size_t)m_srcf);
            uint64_t m_objf = modtime(bd, objfs->s[i]);
            BD_VERBOSE(bd, "modified time of object '%s' = %zu", objfs->s[i], (size_t)m_objf);
            if(m_objf >= m_srcf) {
                /* check dependencies */
                bool recompiled = false;
                StrArr *hdrfs = parse_dfile(bd, depfs->s[i]);
                for(int j = 0; hdrfs && j < hdrfs->n && !bd->error; j++) {
                    uint64_t m_hdrf = modtime(bd, hdrfs->s[j]);
                    BD_VERBOSE(bd, "modified time of header '%s' = %zu", hdrfs->s[j], (size_t)m_hdrf);
                    if(m_hdrf > m_objf) {
                        /* header file was updated, recompile */
                        compile(bd, p, targets->s[k], objfs->s[i], srcfs->s[i]);
                        relink |= true;
                        recompiled = true;
                        break;
                    } 
                }
                if(!recompiled && (newlink || p->type != BUILD_EXAMPLES)) {
                    /* compilation up to date, but it should re-link */
                    if(!strarr_set_n(&bd->ofiles, bd->ofiles.n + 1)) BD_ERR(bd,, "Failed to modify StrArr");
                    bd->ofiles.s[bd->ofiles.n - 1] = strprf(0, "%s", objfs->s[i]);
                }
                strarr_free_p(hdrfs);
            } else {
                compile(bd, p, targets->s[k], objfs->s[i], srcfs->s[i]);
                relink |= true;
            }
            /* only if we're of type EXAMPLES, link already */
            if(p->type == BUILD_EXAMPLES) link(bd, p, targets->s[k], false);
        }
    }
    if(p->type != BUILD_EXAMPLES) link(bd, p, targets->s[0], !relink);
    /* clean up memory used */
    strarr_free_pa(dirn, diro, srcfs, objfs, depfs, targets);
    return;
}

static StrArr *prj_srcfs(Bd *bd, Prj *p)
{
    StrArr *result = 0;
    for(int k = 0; k < p->srcf.n && !bd->error; k++) {
        char *cmd = strprf(0, FIND(p->srcf.s[k]));
        bool state = parse_pipe(bd, cmd, &result);
        if(!state) BD_ERR(bd, 0, "Pipe returned nothing");
        free(cmd);
    }
    return result;
}
static StrArr *prj_names(Bd *bd, Prj *p, StrArr *srcfs)
{
    StrArr *result = strarr_new();
    if(!result) BD_ERR(bd, 0, "Failed to create StrArr");
    /* only if we're not dealing with examples, the name is simple */
    if(p->type != BUILD_EXAMPLES) {
        if(!strarr_set_n(result, result->n + 1)) BD_ERR(bd, 0, "Failed to modify StrArr");
        result->s[result->n - 1] = strprf(0, "%s", p->name ? p->name : "a");
    } else {
        for(int i = 0; i < srcfs->n; i++) {
            int ext = strrstr(srcfs->s[i], ".");
            int dir = strrstr(srcfs->s[i], SLASH_STR);
            /* add to result */
            if(!strarr_set_n(result, result->n + 1)) BD_ERR(bd, 0, "Failed to modify StrArr");
            result->s[result->n - 1] = strprf(0, "%s%s%.*s", p->name ? p->name : "", p->name ? SLASH_STR : "", ext - dir - 1, &srcfs->s[i][dir + 1]);
        }
    }
    return result;
}
static StrArr *prj_srcfs_chg_dirext(Bd *bd, StrArr *srcfs, char *new_dir, char *new_ext)
{
    StrArr *result = strarr_new();
    if(!result) BD_ERR(bd, 0, "Failed to create StrArr");
    if(!strarr_set_n(result, srcfs->n)) BD_ERR(bd, 0, "Failed to modify StrArr");
    for(int i = 0; i < srcfs->n; i++) {
        int ext = strrstr(srcfs->s[i], ".");
        int slash = strrstr(srcfs->s[i], SLASH_STR);
        result->s[i] = strprf(0, "%s%s%.*s%s", new_dir ? new_dir : "", new_dir ? SLASH_STR : "", ext - slash - 1, &srcfs->s[i][slash + 1], new_ext);
    }
    return result;
}

static void clean(Bd *bd, Prj *p)
{
#if defined(OS_WIN)
    char delfilestr[] = "del /q";
    char delfoldstr[] = "rmdir";
    char noerr[] = "2>nul";
#elif defined(OS_CYGWIN) || defined(OS_APPLE) || defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_POSIX)
    char delfilestr[] = "rm";
    char delfoldstr[] = "rm -d";
    char noerr[] = "2>/dev/null";
#endif
    /* gather all files */
    StrArr *dirn = extract_dirs(bd, p->name, (bool)(p->type != BUILD_EXAMPLES));
    if(!dirn) BD_ERR(bd,, "Failed to get directories from name");
    StrArr *diro = extract_dirs(bd, p->objd, false);
    if(!diro) BD_ERR(bd,, "Failed to get directories from objd");
    StrArr *srcfs = prj_srcfs(bd, p);
    if(!srcfs) BD_ERR(bd,, "No source files");
    StrArr *objfs = prj_srcfs_chg_dirext(bd, srcfs, p->objd, ".o");
    if(!objfs) BD_ERR(bd,, "No object files");
    StrArr *depfs = prj_srcfs_chg_dirext(bd, srcfs, p->objd, ".d");
    if(!depfs) BD_ERR(bd,, "No dependency files");
    StrArr *targets = prj_names(bd, p, srcfs);
    if(!targets) BD_ERR(bd,, "No targets to build");
    /* delete all files */
    for(int k = 0; k < targets->n; k++) {
        /* maybe check if target even exists */
        char *targetstr = strprf(0, "%s%s", targets->s[k], static_ext[p->type]);
        char *delfiles = strprf(0, "%s \"%s\" ", delfilestr, targetstr);
        char *delfolds = strprf(0, "%s ", delfoldstr);
        free(targetstr);
        /* set up loop */
        int i0 = (p->type == BUILD_EXAMPLES) ? k : 0;
        int iE = (p->type == BUILD_EXAMPLES) ? k + 1 : srcfs->n;
        for(int i = i0; i < iE; i++) delfiles = strprf(delfiles, "\"%s\" \"%s\" ", objfs->s[i], depfs->s[i]);
        for(int i = dirn->n - 1; i + 1 > 0; i--) delfolds = strprf(delfolds, "\"%s\" ", dirn->s[i]);
        for(int i = diro->n - 1; i + 1 > 0; i--) delfolds = strprf(delfolds, "\"%s\" ", diro->s[i]);
        /* now delete */
        delfiles = strprf(delfiles, noerr);
        delfolds = strprf(delfolds, noerr);
        BD_MSG(bd, "\033[95;1m[ %s ]\033[0m %s", targets->s[k], delfiles); /* bright magenta */
        int sysret = system(delfiles);
        if(sysret == -1) BD_ERR(bd,, "System failed deleting files");
        BD_MSG(bd, "\033[95;1m[ %s ]\033[0m %s", targets->s[k], delfolds); /* bright magenta */
        sysret = system(delfolds);
        if(sysret == -1) BD_ERR(bd,, "System failed deleting folders");
        free(delfiles);
        free(delfolds);
    }
    /* clean up memory used */
    strarr_free_pa(dirn, diro, srcfs, objfs, depfs, targets);
}

static void bd_execute(Bd *bd, CmdList cmd)
{
    Prj p[] = {
#include CONFIG
    };

    switch(cmd) {
        case CMD_BUILD: {
            for(int i = 0; i < (int)SIZE_ARRAY(p); i++) build(bd, &p[i]);
            bd->done = true;
        } break;
        case CMD_CLEAN: {
            for(int i = 0; i < (int)SIZE_ARRAY(p); i++) clean(bd, &p[i]);
            bd->done = true;
        } break;
        case CMD_LIST: {
            for(int i = 0; i < (int)SIZE_ARRAY(p); i++) prj_print(bd, &p[i], true);
            bd->done = true;
        } break;
        case CMD_CONFIG: {
            for(int i = 0; i < (int)SIZE_ARRAY(p); i++) prj_print(bd, &p[i], false);
            bd->done = true;
        } break;
        case CMD_OS: {
            printf(OS_STR"\n");
            bd->done = true;
        } break;
        case CMD_HELP: {
            for(int i = 0; i < CMD__COUNT; i++) printf("%2s%-8s%s\n", "", static_cmds[i], static_cmdsinfo[i]);
            bd->done = true;
        } break;
        case CMD_QUIET: {
            bd->quiet = true;
        } break;
        case CMD_NOERR: {
            bd->noerr = true;
        } break;
        case CMD_VERBOSE: {
            bd->verbose = true;
        } break;
        default: break;
    }
    if(bd->error) BD_ERR(bd,, "an error occured");
}

/* TODO maybe add multithreading */
/* TODO fix potential bug: not checking if file was deleted */
/* TODO add assembly support */
/* TODO maybe avoid compilation of equal files in same project e.g. D("*.c", "*.c")*/

/* start of program */
int main(int argc, const char **argv)
{
    Bd bd = {0};
    /* go over command line args */
    for(int i = 1; i < argc; i++) {
        for(CmdList j = 0; j < CMD__COUNT; j++) {
            if(strcmp(argv[i], static_cmds[j])) continue;
            bd_execute(&bd, j);
        }
    }
    if(!bd.done) bd_execute(&bd, CMD_BUILD);
    return bd.error;
}
