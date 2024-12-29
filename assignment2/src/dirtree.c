//--------------------------------------------------------------------------------------------------
// System Programming                         I/O Lab                                     Fall 2023
//
/// @file  dirtree.c
/// @brief resursively traverse directory tree and list all entries
/// @author yeonjae kim
/// @studid 2020-15607
//--------------------------------------------------------------------------------------------------

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>

#define MAX_DIR 64            ///< maximum number of supported directories

/// @brief output control flags
#define F_TREE      0x1       ///< enable tree view
#define F_SUMMARY   0x2       ///< enable summary
#define F_VERBOSE   0x4       ///< turn on verbose mode

/// @brief struct holding the summary
struct summary {
  unsigned int dirs;          ///< number of directories encountered
  unsigned int files;         ///< number of files
  unsigned int links;         ///< number of links
  unsigned int fifos;         ///< number of pipes
  unsigned int socks;         ///< number of sockets

  unsigned long long size;    ///< total size (in bytes)
  unsigned long long blocks;  ///< total number of blocks (512 byte blocks)
};


/// @brief abort the program with EXIT_FAILURE and an optional error message
///
/// @param msg optional error message or NULL
void panic(const char *msg)
{
  if (msg) fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}


/// @brief read next directory entry from open directory 'dir'. Ignores '.' and '..' entries
///
/// @param dir open DIR* stream
/// @retval entry on success
/// @retval NULL on error or if there are no more entries
struct dirent *getNext(DIR *dir)
{
  struct dirent *next;
  int ignore;

  do {
    errno = 0;
    next = readdir(dir);
    if (errno != 0) perror(NULL);
    ignore = next && ((strcmp(next->d_name, ".") == 0) || (strcmp(next->d_name, "..") == 0));
  } while (next && ignore);

  return next;
}


/// @brief qsort comparator to sort directory entries. Sorted by name, directories first.
///
/// @param a pointer to first entry
/// @param b pointer to second entry
/// @retval -1 if a<b
/// @retval 0  if a==b
/// @retval 1  if a>b
static int dirent_compare(const void *a, const void *b)
{
  struct dirent *e1 = *(struct dirent**)a;
  struct dirent *e2 = *(struct dirent**)b;

  // if one of the entries is a directory, it comes first
  if (e1->d_type != e2->d_type) {
    if (e1->d_type == DT_DIR) return -1;
    if (e2->d_type == DT_DIR) return 1;
  }

  // otherwise sorty by name
  return strcmp(e1->d_name, e2->d_name);
}

static char* countFile(int c, const char* file) {
    char* result = NULL;
    int asprintfError;

    if (c == 1) {
      if((asprintfError = asprintf(&result, "%d %s", c, file)) < 0){
        panic("asprintf Error");
      }
    } 
    else if(strcmp(file,"directory") == 0){
      if((asprintfError = asprintf(&result, "%d directories", c)) < 0){
        panic("asprintf Error");
      }
    }
    else {
      if((asprintfError = asprintf(&result, "%d %ss", c, file)) < 0){
        panic("asprintf Error");
      }
    }

    return result;
}

static char* printSum(struct summary sum){
  char* files = countFile(sum.files,"file");
  char* dirs = countFile(sum.dirs,"directory");
  char* links = countFile(sum.links,"link");
  char* pipes = countFile(sum.fifos,"pipe");
  char* sockets = countFile(sum.socks,"socket");

  char* result = NULL;

  int asprintfError;

  if((asprintfError = asprintf(&result, "%s, %s, %s, %s, and %s",files,dirs,links,pipes,sockets)) < 0){
    panic("asprintf error");
  }

  free(files);
  free(dirs);
  free(links);
  free(pipes);
  free(sockets);

  return result;

}

static void fileToSum(struct dirent* file, struct summary * stat, struct stat sb){
    stat->size += sb.st_size;
    stat->blocks += sb.st_blocks;
    switch(file->d_type){
        case DT_REG     : stat->files++; break;
        case DT_LNK     : stat->links++; break;
        case DT_SOCK    : stat->socks++; break;
        case DT_FIFO    : stat->fifos++; break;
        case DT_DIR     : stat->dirs++; break;
        default         : break;
    }
}

static char typeToChar(int type){
    switch(type){
        case DT_REG     : return ' ';
        case DT_LNK     : return 'l';
        case DT_BLK     : return 'b';
        case DT_SOCK    : return 's';
        case DT_FIFO    : return 'f';
        case DT_DIR     : return 'd';
        case DT_CHR     : return 'c';
        default         : return EXIT_FAILURE;
    }
}


/// @brief recursively process directory @a dn and print its tree
///
/// @param dn absolute or relative path string
/// @param pstr prefix string printed in front of each entry
/// @param stats pointer to statistics
/// @param flags output control flags (F_*)
void processDir(const char *dn, const char *pstr, struct summary *stats, unsigned int flags)
{
  // TODO
  DIR *dir = opendir(dn);
  if(dir == NULL){
    if(flags & F_TREE){
      printf("%s`-ERROR: Permission denied\n",pstr);
    }
    else{
      printf("%s  ERROR: Permission denied\n",pstr);
    }
    return;
  }

  int dd = dirfd(dir);

  errno = 0;
  struct dirent* files[MAX_DIR];
  struct dirent *e;
  int count = 0;

  int asprintfError;
  
  while((e = getNext(dir)) != NULL){
    files[count++] = e;
  }

  if(errno !=0) panic("What is error");

  qsort(files, count, sizeof(struct dirent* ), dirent_compare);

  char* name = (char*)malloc(55);
  for(int j = 0 ; j < count; j++){
    memset(name,0,55);
    strcpy(name,pstr);
    char* dir_pstr;
    size_t length = strlen(pstr) + 2/*strlen("  ")*/ + 1;
    dir_pstr = (char*)malloc(length);
    strcpy(dir_pstr,pstr);
    if(flags & F_TREE){
      if(j == count - 1){
        strcat(name,"`-");
        strcat(dir_pstr,"  ");
        // if(strlen(pstr)==0) strcpy(dir_pstr,"` ");
        // else {strcpy(dir_pstr,pstr); strcat(dir_pstr,"  ");}
      }
      else{
        strcat(name,"|-");
        strcat(dir_pstr,"| ");
        // if(strlen(pstr)==0) strcpy(dir_pstr,"| ");
        // else {strcpy(dir_pstr,pstr); strcat(dir_pstr,"  ");}
      }
    }
    else{
      strcat(name,"  ");
      strcpy(dir_pstr,pstr);
      strcat(dir_pstr,"  ");
    }
    
  
    if(strlen(name) + strlen(files[j] -> d_name) > 54){
      size_t remaining_space = 51 - strlen(name);
      strncat(name, files[j]->d_name, remaining_space);
      strcat(name, "...");
    }
    else{
      strcat(name,files[j]->d_name);
    }

    struct stat sb;
    struct passwd *pwd;
    struct group *grp;
    if(files[j] -> d_type == DT_LNK){
        char* filePathAndName;
        if((asprintfError = asprintf(&filePathAndName,"%s/%s",dn,files[j]->d_name)) < 0){
          panic("asprintf Error");
        }
        if(lstat(filePathAndName,&sb) < 0){
            panic("Cannot stat link");
        }
        free(filePathAndName);
    }
    else{
        if(fstatat(dd, files[j]->d_name, &sb, 0)<0){
            panic("Cannot stat file");
        }
    }

    pwd = getpwuid(sb.st_uid);
    if(pwd == NULL){
        panic("no pwd");
    }

    grp = getgrgid(sb.st_gid);
    if(grp == NULL){
        panic("no grp");
    }

    fileToSum(files[j],stats,sb);

    if(flags & F_VERBOSE){
        printf("%-54s  %8.8s:%-8.8s  %10ld  %8ld  %c\n",name,pwd->pw_name,grp->gr_name,sb.st_size,sb.st_blocks,typeToChar(files[j]->d_type));
    }
    else{
      printf("%s\n",name);
    }

    if(files[j]->d_type == DT_DIR){
      char* new_dn ;
      size_t length_dn =strlen(dn) + strlen(files[j]->d_name) + 2;

      new_dn = (char*)malloc(length_dn);
      strcpy(new_dn,dn);
      strcat(new_dn,"/");
      strcat(new_dn,files[j]->d_name);

      processDir(new_dn, dir_pstr, stats, flags);
    }
    
  }
  free(name);
  closedir(dir);

}

static void sumSummary(struct summary a, struct summary* b){
  b->files  += a.files;
  b->dirs   += a.dirs;
  b->fifos  += a.fifos;
  b->blocks += a.blocks;
  b->links  += a.links;
  b->size   += a.size;
  b->socks  += a.socks;
}


/// @brief print program syntax and an optional error message. Aborts the program with EXIT_FAILURE
///
/// @param argv0 command line argument 0 (executable)
/// @param error optional error (format) string (printf format) or NULL
/// @param ... parameter to the error format string
void syntax(const char *argv0, const char *error, ...)
{
  if (error) {
    va_list ap;

    va_start(ap, error);
    vfprintf(stderr, error, ap);
    va_end(ap);

    printf("\n\n");
  }

  assert(argv0 != NULL);

  fprintf(stderr, "Usage %s [-t] [-s] [-v] [-h] [path...]\n"
                  "Gather information about directory trees. If no path is given, the current directory\n"
                  "is analyzed.\n"
                  "\n"
                  "Options:\n"
                  " -t        print the directory tree (default if no other option specified)\n"
                  " -s        print summary of directories (total number of files, total file size, etc)\n"
                  " -v        print detailed information for each file. Turns on tree view.\n"
                  " -h        print this help\n"
                  " path...   list of space-separated paths (max %d). Default is the current directory.\n",
                  basename(argv0), MAX_DIR);

  exit(EXIT_FAILURE);
}


/// @brief program entry point
int main(int argc, char *argv[])
{
  //
  // default directory is the current directory (".")
  //
  const char CURDIR[] = ".";
  const char *directories[MAX_DIR];
  int   ndir = 0;

  struct summary tstat;
  unsigned int flags = 0;

  //
  // parse arguments
  //
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      // format: "-<flag>"
      if      (!strcmp(argv[i], "-t")) flags |= F_TREE;
      else if (!strcmp(argv[i], "-s")) flags |= F_SUMMARY;
      else if (!strcmp(argv[i], "-v")) flags |= F_VERBOSE;
      else if (!strcmp(argv[i], "-h")) syntax(argv[0], NULL);
      else syntax(argv[0], "Unrecognized option '%s'.", argv[i]);
    } else {
      // anything else is recognized as a directory
      if (ndir < MAX_DIR) {
        directories[ndir++] = argv[i];
      } else {
        printf("Warning: maximum number of directories exceeded, ignoring '%s'.\n", argv[i]);
      }
    }
  }

  // if no directory was specified, use the current directory
  if (ndir == 0) directories[ndir++] = CURDIR;


  //
  // process each directory
  //
  // TODO
  //
  // Pseudo-code
  // - reset statistics (tstat)
  // - loop over all entries in 'directories' (number of entires stored in 'ndir')
  //   - reset statistics (dstat)
  //   - if F_SUMMARY flag set: print header
  //   - print directory name
  //   - call processDir() for the directory
  //   - if F_SUMMARY flag set: print summary & update statistics
  memset(&tstat, 0, sizeof(tstat));

  struct summary d_stat;
  //...
  for(int i = 0 ;i < ndir ; i++){
    memset(&d_stat, 0, sizeof(d_stat));
    if(flags & F_SUMMARY){
      if(flags & F_VERBOSE){
        printf("%-54s  %8.8s:%-8.8s  %10s  %8s %s \n","Name","User","Group","Size","Blocks","Type");
      }
      else{
        printf("Name\n");
      }
      for(int j = 0 ;j <100;j++){
        printf("-");
      }
      printf("\n");
    }
    printf("%s\n",directories[i]);
    processDir(directories[i],"", &d_stat, flags);
    if(flags & F_SUMMARY){
      for(int j = 0 ;j <100;j++){
        printf("-");
      }
      printf("\n");
      char* summary = printSum(d_stat);
      if(flags & F_VERBOSE){
        printf("%-68s   %14lld %9lld\n",summary,d_stat.size,d_stat.blocks);
      }
      else{
        printf("%-68s\n",summary);
      }
      printf("\n");
      
      free(summary);
      sumSummary(d_stat,&tstat);
    }
  }

  


  //
  // print grand total
  //
  if ((flags & F_SUMMARY) && (ndir > 1)) {
    printf("Analyzed %d directories:\n"
           "  total # of files:        %16d\n"
           "  total # of directories:  %16d\n"
           "  total # of links:        %16d\n"
           "  total # of pipes:        %16d\n"
           "  total # of sockets:      %16d\n",
           ndir, tstat.files, tstat.dirs, tstat.links, tstat.fifos, tstat.socks);

    if (flags & F_VERBOSE) {
      printf("  total file size:         %16llu\n"
             "  total # of blocks:       %16llu\n",
             tstat.size, tstat.blocks);
    }

  }

  //
  // that's all, folks!
  //
  return EXIT_SUCCESS;
}

