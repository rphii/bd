#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <inttypes.h> // maybe remove when finished
#include <dirent.h>
#include <time.h>
#include <stdbool.h>

/* start of configuration */
#define APPNAME   "a"
#define VERSION   "1.00"
#define CC        "gcc"
#define CFLAGS    "-Wall -c"
#define LDFLAGS   ""
#define LBFLAGS   ""
#define CSUFFIX	".c"
#define HSUFFIX	".h"
#define SRC_DIR   "src"
#define OBJ_DIR   "obj"
/* end of configuration */

/* start of shared defines */
#define PRE_O     "-MMD -o"
#define SIZE_ARRAY(x)   (sizeof(x)/sizeof(*x))
#define SIZE_STR(x)     (SIZE_ARRAY(x) - 1)
#define BUF_CC          (2*BUF_FILENAMES + SIZE_STR(CC) + SIZE_STR(CFLAGS) + SIZE_STR(PRE_O) + SIZE_STR(SRC_DIR) + SIZE_STR(OBJ_DIR) + 7)  /* 4 for spaces, 2 for '/' in DIRs, 1 for '\0' */
/* end of shared defines*/

/* start of os detection */
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
   // define something for Windows (32-bit and 64-bit, this part is common)
   #define OS_STR "Windows"
   #define OS_WIN
   #ifdef _WIN64
   #include "windows.h"
   #include "fileapi.h"
   #include "errhandlingapi.h"
   #define BUF_FILENAMES   260
      // define something for Windows (64-bit only)
   #else
      // define something for Windows (32-bit only)
   #endif
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

typedef enum {
   CMD_BUILD,
   CMD_CLEAN,
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
   "os",
   "-h",
   "-q",
   "-e",
};

static const char *cmdsinfo[CMD__COUNT] = {
   "Build the application",
   "Clean created files",
   "Print the Operating System",
   "Help output (this here)",
   "Execute quietly",
   "Also makes errors quiet",
};

typedef struct {
   int error;
   int count;
   char (*ofiles)[BUF_FILENAMES];
   bool quiet;
   bool noerr;
   bool done;
} Bd;

static void bd_msg(Bd *bd, const char *msg)
{
   if(!bd->quiet) printf("%s\n", msg);
}
#define BD_ERR(bd,...) if(!bd->noerr) { printf("\033[91m[ERROR:%d]\033[0m ", __LINE__); printf(__VA_ARGS__); }

static ssize_t file_read(const char *filename, char **dump)
{
   if(!filename) return -1;
   FILE *file = fopen(filename, "rb");
   if(!file) return 0;

   // get file length 
   fseek(file, 0, SEEK_END);
   size_t bytes_file = ftell(file);
   fseek(file, 0, SEEK_SET);

   // allocate memory
   if(dump)
   {
      char *temp = realloc(*dump, bytes_file + 1);
      if(!temp) return -1;
      *dump = temp;

      // read file
      size_t bytes_read = fread(*dump, 1, bytes_file, file);
      if(bytes_file != bytes_read) return 0;
      (*dump)[bytes_read] = 0;
   }
   // close file
   fclose(file);
   return bytes_file;
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

static int dep_onefile(const char *dep)
{
   int result = 0;
   while(dep[result]) {
      if(dep[result] == '\\') {
         result++;
      } else if(dep[result] == ' ' || dep[result] == '\r' || dep[result] == '\n') {
         break;
      }
      result++;
   }
   return result;
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

static void compile(Bd *bd, const char file_c[BUF_FILENAMES], const char file_o[BUF_FILENAMES])
{
   char cc[BUF_CC] = {0};
   snprintf(cc, BUF_CC, CC" "CFLAGS" "PRE_O" %s %s", file_o, file_c);
   bd_msg(bd, cc);
   bd->error = system(cc);
   if(bd->error) return;
   /* add filename to bd */
   void *temp = realloc(bd->ofiles, sizeof(*bd->ofiles) * (bd->count + 1));
   if(!temp) {
      bd->error = __LINE__;
      return;
   }
   bd->ofiles = temp;
   snprintf(bd->ofiles[bd->count++], BUF_FILENAMES, file_o);
}

static void link(Bd *bd)
{
   if(!bd->error && bd->count)
   {
      /* link */
      size_t len = 0;
      for(int i = 0; i < bd->count; i++)
      {
         len += strlen(bd->ofiles[i]) + 1; /* 1 for space */
      }
      const char ldflags[] = CC" "LDFLAGS" -o "APPNAME;
      const char lbflags[] = " "LBFLAGS;
      len += strlen(ldflags) + strlen(lbflags) + 1; /* 1 for terminating character */
      char *cc = malloc(len);
      if(!cc) {
         bd->error = __LINE__;
         return;
      }
      size_t pos = snprintf(cc, len, "%s", ldflags);
      for(int i = 0; i < bd->count; i++)
      {
         pos += snprintf(&cc[pos], len - pos, " %s", bd->ofiles[i]);
      }
      snprintf(&cc[pos], len - pos, "%s", lbflags);
      bd_msg(bd, cc);
      system(cc);
   }
   /* finally also free up memory */
   free(bd->ofiles);
   bd->ofiles = 0;
}

static uint64_t modtime(Bd *bd, const char filename[BUF_FILENAMES])
{
#if defined(OS_WIN)
   HANDLE filehandle = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
   if(filehandle == INVALID_HANDLE_VALUE) {
      DWORD lasterr = GetLastError();
      if(lasterr != ERROR_FILE_NOT_FOUND) {
         BD_ERR(bd, "%s: Failed to get handle (code %ld)\n", filename, lasterr);
      }
      return 0;
   }
   FILETIME t;
   BOOL gottime = GetFileTime(filehandle, 0, 0, &t);
   if(!gottime) {
      BD_ERR(bd, "%s: Failed to retrieve time information (code %ld)\n", filename, GetLastError());
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

static void build(Bd *bd)
{
   DIR *src_dir = opendir(SRC_DIR);
   if(!src_dir) {
      BD_ERR(bd, "Could not open directory `%s`\n", SRC_DIR);
      return;
   }

   bool newbuild = false;

   // TODO make this a function like, exist_dir?!
#if defined(OS_WIN)
   newbuild = (file_read(APPNAME".exe", 0) == 0);
#elif defined(OS_LINUX)
   newbuild = (file_read(APPNAME, 0) == 0);
#endif
   
   printf("newbuild=%s\n", newbuild?"true":"false");
   makedir(OBJ_DIR);
   struct dirent *dp = 0;
   while(!bd->error && (dp = readdir(src_dir)) != NULL) {
      const char *filename = dp->d_name;
      printf("looping: %s\n", filename);
      if((filename[0] == '.' && !filename[1]) ||
         (filename[0] == '.' && filename[1] == '.' && !filename[2])) {
         continue;
      }

      /* get last position of c suffix */
      int csuffix_pos = strrstrn(filename, CSUFFIX); // TODO check if it is REALLY at the end of the filename... (eg. something.c.test wouldn't be good)
      if(csuffix_pos < 0) {
         continue;
      }

      /* construct more filename strings */
      char filename_c[BUF_FILENAMES] = {0};
      char filename_o[BUF_FILENAMES] = {0};
      snprintf(filename_c, BUF_FILENAMES, SRC_DIR"/%s", filename);
      snprintf(filename_o, BUF_FILENAMES, OBJ_DIR"/%.*s.o", csuffix_pos, filename);

      /* get their modification times */
      uint64_t mod_c = modtime(bd, filename_c);
      uint64_t mod_o = modtime(bd, filename_o);

      if(!newbuild && mod_o >= mod_c) { 
         /* read dependencies */
         char filename_d[BUF_FILENAMES] = {0};
         snprintf(filename_d, BUF_FILENAMES, OBJ_DIR"/%.*s.d", csuffix_pos, filename);


         char *filecontent_d = 0;
         ssize_t bytes_d = file_read(filename_d, &filecontent_d); // TODO check bytes_d
         if(bytes_d < 0) {
            BD_ERR(bd, "`%s`: could not read file\n", filename_d);
         }
         /* check dependencies */
         const char csuffix[] = ".c ";
         char *filename_deps = strstr(filecontent_d, csuffix);
         if(!filename_deps) continue;

         filename_deps += SIZE_STR(csuffix);
         while(*filename_deps) {
            int len_dep = dep_onefile(filename_deps);
            if(!len_dep) break;
            char filename_dep[BUF_FILENAMES] = {0};
            snprintf(filename_dep, BUF_FILENAMES, "%.*s", len_dep, filename_deps); // TODO fix bug with "more characters in filename" when it's like "      " (many spaces)
            uint64_t mod_dep = modtime(bd, filename_dep);
            if(mod_dep > mod_o) {
               compile(bd, filename_c, filename_o);
               break;
            }
            filename_deps += len_dep + 1;
         }
         free(filecontent_d);
         continue;
      } else {
         compile(bd, filename_c, filename_o);
      }
   }
   /* finally link */
   link(bd);
}

static void clean(Bd *bd)
{
#if defined(OS_WIN)
   const char delfiles[] = "del /q "OBJ_DIR" "APPNAME".exe 2>nul";
   const char delfolder[] = "rmdir "OBJ_DIR" 2>nul";
   bd_msg(bd, delfiles);
   system(delfiles);
   bd_msg(bd, delfolder);
   system(delfolder);
#elif defined(OS_LINUX)
   const char del[] = "rm -rf "OBJ_DIR" "APPNAME;
   bd_msg(bd, del);
   system(del);
#endif
}

static void bd_execute(Bd *bd, CmdList cmd)
{
   switch(cmd)
   {
      case CMD_BUILD: {
         build(bd);
         bd->done = true;
      } break;
      case CMD_CLEAN: {
         clean(bd);
         bd->done = true;
      } break;
      case CMD_OS: {
         printf(OS_STR"\n");
         bd->done = true;
      } break;
      case CMD_HELP: {
         for(int i = 0; i < CMD__COUNT; i++) printf("%-8s%s\n", cmds[i], cmdsinfo[i]);
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

   // go over command line args
   for(int i = 1; i < argc; i++) {
      for(CmdList j = 0; j < CMD__COUNT; j++) {
         if(strcmp(argv[i], cmds[j])) continue;
         bd_execute(&bd, j);
      }
   }
   if(!bd.done) bd_execute(&bd, CMD_BUILD);
   return bd.error;
}

