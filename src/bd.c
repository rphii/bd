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
    #define OS_WIN
    #define SLASH_STR   "\\"
    #define SLASH_CH    '\\'
    #define EXT_STR ".exe"
    #ifdef _WIN64
    #include "windows.h"
    #include "fileapi.h"
    #include "errhandlingapi.h"
    #define BUF_FILENAMES   260
        // define something for Windows (64-bit only)
    #else
        // define something for Windows (32-bit only)
    #endif
#else
    /* common things by all others */
    #define SLASH_STR   "/"
    #define SLASH_CH    '/'
    #define EXT_STR ""
#endif
#if defined(OS_WIN)
#elif defined(__CYGWIN__)
    #define OS_STR "Cygwin"
    #define OS_CYGWIN
    #define BUF_FILENAMES   256
#elif __APPLE__
    #define OS_STR "Apple"
    #define OS_APPLE
    #define BUF_FILENAMES   256
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
    #define OS_ANDROID
    #define BUF_FILENAMES   256
    // Below __linux__ check should be enough to handle Android,
    // but something may be unique to Android.
#elif __linux__
    #define OS_STR "Linux"
    #define OS_LINUX
    #define BUF_FILENAMES   256
    // linux
#elif __unix__ // all unices not caught above
    #define OS_STR "Apple"
    #define OS_UNIX
    #define BUF_FILENAMES   256
    // Unix
#elif defined(_POSIX_VERSION)
    #define OS_STR "Posix"
    #define OS_POSIX
    #define BUF_FILENAMES   256
    // POSIX
#else
#   error "Unknown compiler"
#endif
/* end of os detection */

/* start of globbing pattern */
#if defined(OS_WIN)
    #define FIND(p) "cmd /V /C \"set \"var= %s\" && set \"var=!var:/=\\!\" && for %%I in (!var!) do @echo %%~dpnxI\"", p
#elif defined(OS_CYGWIN)
    #define FIND(p) "find ~+/$(dirname \"%s\") -maxdepth 1 -type f -name $(basename \"%s\")", p, p
#endif
/* end of globbing pattern */

#define D(...)          (StrArr){.s = (char *[]){__VA_ARGS__}, .n = sizeof((char *[]){__VA_ARGS__})/sizeof(*(char *[]){__VA_ARGS__})}
#define ERR(...)        if(1) { printf("\033[91m[ERROR:%d]\033[0m ", __LINE__); printf(__VA_ARGS__); printf("\n"); }
#define BD_ERR(bd,...)  if(!bd->noerr) { printf("\033[91m[ERROR:%d]\033[0m ", __LINE__); printf(__VA_ARGS__); printf("\n"); }
#define BD_MSG(bd,...)  if(!bd->quiet) { printf(__VA_ARGS__); printf("\n"); }
#define SIZE_ARRAY(x)   (sizeof(x)/sizeof(*x))

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
    "List all configured projects",
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
    TYPE_APP,
    TYPE_STATIC,
    TYPE_SHARED,
    TYPE__COUNT,
} TypeList;
static char *static_cc[TYPE__COUNT] = {
    "gcc -c -MMD -MP %s -o %s %s", // flags, ofile, cfile
    "gcc -c -MMD -MP %s -o %s %s", // flags, ofile, cfile
    "gcc -c -MMD -MP -fPIC %s -o %s %s", // flags, ofile, cfile
};
static char *static_ld[TYPE__COUNT] = {
    "gcc %s -o %s %s %s", // options, name, ofiles, libstuff
    "ar rcs lib%s %s",  // name[.a], ofiles
    "gcc -shared -fPIC %s -o lib%s %s %s", // options, name[.so/.dll], ofiles, more options
};
static const char *type_str[] = {
    "App",
    "Static",
    "Shared",
};

typedef struct Prj {
    char *cflgs;    /* compile flags / options */   
    char *lopts;    /* linker flags / options */
    char *llibs;    /* linker library */
    char *name;     /* name of the thing */
    char *objd;     /* object directory */
    StrArr srcf;    /* source files */
    TypeList type;  /* type */
} Prj;

static void prj_print(Prj *p, bool simple)
{
    if(!p) return;
    printf("[%s] : %s\n", p->name, type_str[p->type]);
    if(simple) return;
    printf("  cflgs = %s\n", p->cflgs);
    printf("  lopts = %s\n", p->lopts);
    printf("  llibs = %s\n", p->llibs);
    printf("  [%s] : \n", p->objd);
    for(int i = 0; i < p->srcf.n; i++) printf("%4s%s\n", "", p->srcf.s[i]);
}

char *strprf(char *format, ...)
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

StrArr *strarr_new()
{
    StrArr *result = malloc(sizeof(*result));
    if(!result) return 0;
    memset(result, 0, sizeof(*result));
    return result;
}

void strarr_free(StrArr *arr)
{
    if(!arr) return;
    for(int i = 0; i < arr->n; i++) free(arr->s[i]);
    free(arr->s);
    arr->s = 0;
    arr->n = 0;
}

void strarr_set_n(StrArr *arr, int n)
{
    if(!arr || !n) return;
    if(arr->n == n) return;
    void *temp = realloc(arr->s, sizeof(*arr->s) * (n));
    if(!temp) return;
    arr->s = temp;
    memset(&arr->s[arr->n], 0, sizeof(*arr->s) * (n - arr->n));
    arr->n = n;
}

int str_append(char **str, char *format, ...)
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

StrArr *parse_pipe(char *cmd)
{
    FILE *fp = popen(cmd, "rb");
    if(!fp) {
        ERR("Could not open pipe");
        return 0;
    }

    // printf("[[%s]]\n", cmd);
    StrArr *result = strarr_new();

    int c = fgetc(fp);
    int n = 1;
    while(c != EOF) {
        if(c == '\n') n++;
        else {
            strarr_set_n(result, n);
            str_append(&result->s[result->n - 1], "%c", c);
        }
        c = fgetc(fp);
    }

    if(pclose(fp)) {
        ERR("Could not close pipe");
        return 0;
    }
    return result;
}

StrArr *parse_dfile(char *dfile)
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
            strarr_set_n(result, n);
            str_append(&result->s[result->n - 1], "%c", c);
        }
        c = fgetc(fp);
    }

    if(fclose(fp)) {
        ERR("Could not close dfile");
        return 0;
    }
    return result;
}

static int strrstrn(const char s1[BUF_FILENAMES], const char *s2)
{
   const int len_s1 = strlen(s1);
   const int len_s2 = strlen(s2);
   for(int i = len_s1 - len_s2; i > 0; i--) {
      if(!strncmp(&s1[i], s2, len_s2)) {
         return i;
      }
   }
   return -1;
}

static uint64_t modtime(Bd *bd, const char *filename)
{
#if defined(OS_WIN)
    HANDLE filehandle = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(filehandle == INVALID_HANDLE_VALUE) {
        DWORD lasterr = GetLastError();
        if(lasterr != ERROR_FILE_NOT_FOUND) {
            BD_ERR(bd, "%s: Failed to get handle (code %ld)", filename, lasterr);
        }
        return 0;
    }
    FILETIME t;
    BOOL gottime = GetFileTime(filehandle, 0, 0, &t);
    if(!gottime) {
        BD_ERR(bd, "%s: Failed to retrieve time information (code %ld)", filename, GetLastError());
        return 0;
    }
    CloseHandle(filehandle);
    ULARGE_INTEGER result = {.HighPart = t.dwHighDateTime, .LowPart = t.dwLowDateTime};
    return result.QuadPart;
#elif defined(OS_LINUX)
    struct stat attr;
    if(stat(filename, &attr) == -1) {
        return 0;
    }
    return (uint64_t)attr.st_ctime;
#endif
}

static void makedir(const char *dirname)
{
#if defined(OS_WIN)
   // TODO make quiet (somehow it is?! or isn't?! idk)
   mkdir(dirname);
#elif defined(OS_LINUX)
   mkdir(dirname, 0700);
#endif
}

static void build(Bd *bd, Prj *p)
{
    if(!bd) return;
    if(bd->error) return;
    
    // TODO make this a function like, exist_dir?!
    char *appstr = strprf("%s"EXT_STR, p->name);
    bool newbuild = (modtime(bd, appstr) == 0);
    free(appstr);

    makedir(p->objd);

    for(int i = 0; i < p->srcf.n && !bd->error; i++) {
        bool compile = false;
        char *cmd = strprf(FIND(p->srcf.s[i]));
        StrArr *res = parse_pipe(cmd);
        for(int i = 0; i < res->n; i++) {
            int ext = strrstrn(res->s[i], ".c");
            int dir = strrstrn(res->s[i], SLASH_STR);
            int len = strlen(res->s[i]);
            char *srcf = strprf("%.*s", len - bd->cutoff - 1, &res->s[i][bd->cutoff]);  /* dangerous ?! */
            // char *srcf = res->s[i];
            char *objf = strprf("%s%.*s.o", p->objd, ext - dir, &res->s[i][dir]);
            char *dfile = strprf("%s%.*s.d", p->objd, ext - dir, &res->s[i][dir]);
            uint64_t modc = modtime(bd, srcf);
            uint64_t modo = modtime(bd, objf);
            if(!newbuild && modo >= modc) {
                /* check .d file in objd */
                StrArr *hfiles = parse_dfile(dfile);
                for(int j = 0; !compile && hfiles && j < hfiles->n; j++) {
                    // printf("[DEP] %s\n", hfiles->s[j]);
                    uint64_t modh = modtime(bd, hfiles->s[j]);
                    if(modh > modo) compile = true;
                }
                strarr_free(hfiles);
            } else {
                compile = true;
            }
            if(compile) {
                char *cc = strprf(static_cc[p->type], p->cflgs ? p->cflgs : "", objf, srcf);
                strarr_set_n(&bd->ofiles, bd->ofiles.n + 1);
                str_append(&bd->ofiles.s[bd->ofiles.n - 1], objf);
                BD_MSG(bd, "[%s] %s", p->name, cc);
                bd->error = system(cc);
                free(cc);
            }

            // printf("%2s%s\n", "", res->s[i]);
            // printf("%2s%s%.*s.o\n", "", p->objd, ext - dir, &res->s[i][dir]);
            free(srcf);
            free(objf);
            free(dfile);
        }
        strarr_free(res);
        free(cmd);
    }

    if(bd->ofiles.n) {
        /* link */
        char *ofiles = 0;
        for(int i = 0; i < bd->ofiles.n; i++) str_append(&ofiles, "%s ", bd->ofiles.s[i]);
        char *ld = strprf(static_ld[p->type], p->lopts ? p->lopts : "", p->name, ofiles, p->llibs ? p->llibs : "");
        BD_MSG(bd, "[%s] %s", p->name, ld);
        bd->error = system(ld);
        free(ofiles);
        free(ld);
    } else {
        BD_MSG(bd, "[%s] is up to date", p->name);
    }
    
    strarr_free(&bd->ofiles);
}

static void clean(Bd *bd, Prj *p)
{
#if defined(OS_WIN)
    char *delfiles = strprf("del /q %s %s.exe 2>nul", p->objd, p->name);
    char *delfolder = strprf("rmdir %s 2>nul", p->objd);
    BD_MSG(bd, "[%s] %s", p->name, delfiles);
    system(delfiles);
    BD_MSG(bd, "[%s] %s", p->name, delfolder);
    system(delfolder);
    free(delfiles);
    free(delfolder);
#elif defined(OS_LINUX)
    char *del = strprf("rm -rf %s %s", p->objd, p->name);
    BD_MSG(bd, "[%s] %s", p->name, del);
    system(del);
#endif
}

static void bd_execute(Bd *bd, CmdList cmd)
{
    Prj p[] = {{
        .name = "Test",
        .type = TYPE_APP,
        .objd = "obj",
        .srcf = D("src/test.c"),
        .cflgs = "-Wall",
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
            for(int i = 0; i < SIZE_ARRAY(p); i++) prj_print(&p[i], true);
            bd->done = true;
        } break;
        case CMD_CONFIG: {
            for(int i = 0; i < SIZE_ARRAY(p); i++) prj_print(&p[i], false);
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
