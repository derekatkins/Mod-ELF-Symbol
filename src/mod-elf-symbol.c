// elf..
#include <elf.h>

// write, getopt..
#include <getopt.h>
#include <unistd.h>

// printf..
#include <stdio.h>

// exit..
#include <stdlib.h>

// assert
#include <assert.h>

// c strings..
#include <string.h>

// for open..
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// utilities
#include "../include/util.h"

// MACROS
#define c_print(x) write(1, x, strlen(x))

// CONST
const int debug = 0;
const int debug_func = 0;

// ENUMERATION
typedef enum { SINGLESYM = 0, KEEPNUMSYM, COMPLETESYM } FLAGTYPE;

typedef enum { BOTH_DEF_AND_UNDEF = 0, ONLY_DEF, ONLY_UNDEF } REPLACETYPE;

static REPLACETYPE def_or_undef = BOTH_DEF_AND_UNDEF;
static int verbose = 0;

static char *actualSingleSymbolsToReplace[300] = {0};
static int actualSingleSymbolsIndex = 0;
static char *actualKeepNumSymbolsToReplace[300] = {0};
static int actualKeepNumSymbolsIndex = 0;
static char *actualCompleteSymbolsToReplace[300] = {0};
static int actualCompleteSymbolsIndex = 0;

// Combined 32/64-bit Elf Header Structure
typedef struct {
  union {
    Elf32_Ehdr elf32_ehdr;
    Elf64_Ehdr elf64_ehdr;
    unsigned char e_ident[EI_NIDENT];
  } ehdr;
  int elfclass;
} Elf_Ehdr;

// Future function implementations:
int readInElfHeader();
int readInSymbolTable();
int readInStringTable();
int readInSectionHeaderTable();

int writeOutElfHeader();
int writeOutSymbolTable();
int writeOutStringTable();
int wrtieOutSectionHeaderTable();

// Helper functions
int runGetOpt(int argc, char **argv, int *objIndex, char **objList,
              int *singleSymbolIndex, char **singleSymbolList,
              int *keepNumSymbolIndex, char **keepNumSymbolList,
              int *completeSymbolIndex, char **completeSymbolList,
              char **singleStrPtr, char **keepNumStrPtr,
              char **completeStrPtr) {
  if (debug_func)
    printf("runGetOpt\n");
  int c;
  int digit_optind = 0;
  while (1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
        {"object", required_argument, 0, 0},
        {"singlesymbol", required_argument, 0, 1},
        {"keepnumsymbol", required_argument, 0, 2},
        {"singlestr", required_argument, 0, 3},
        {"keepnumstr", required_argument, 0, 4},
        {"completesymbol", required_argument, 0, 7},
        {"completestr", required_argument, 0, 8},
        {"only_def", no_argument, 0, 9},
        {"only_undef", no_argument, 0, 10},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0}};

    c = getopt_long(argc, argv, "o:s:k:c:v", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      printf("option %s", long_options[option_index].name);
      if (optarg)
        printf(" with arg %s", optarg);
      printf("\n");

    case 0: // object
      objList[(*objIndex)++] = strdup(optarg);
      break;
    case 'o':
      optind--;
      do {
        objList[(*objIndex)++] = strdup(argv[optind]);
        optind++;
      } while (optind < argc && *argv[optind] != '-');
      break;

    case 1: // single symbol
      singleSymbolList[(*singleSymbolIndex)++] = strdup(optarg);
      break;
    case 's':
      optind--;
      do {
        singleSymbolList[(*singleSymbolIndex)++] = strdup(argv[optind]);
        optind++;
      } while (optind < argc && *argv[optind] != '-');
      break;

    case 2: // multiple symbol requiring numbering
      if (objIndex == 0) {
        printf("*** ***If need to assign numbering to symbol, list object "
               "files first!\n");
        perror("Syntax: ./replace-symbols-name -o <obj file> [obj files] "
               "--keepnumsymbol=<symbol>");
        exit(1);
      }
      keepNumSymbolList[(*keepNumSymbolIndex)++] = strdup(optarg);
      break;
    case 'k':
      if (objIndex == 0) {
        printf("*** ***If need to assign numbering to symbol, list object "
               "files first!\n");
        perror("Syntax: ./replace-symbols-name -o <obj file> [obj files] "
               "--keepnumsymbol=<symbol>");
        exit(1);
      }
      optind--;
      do {
        keepNumSymbolList[(*keepNumSymbolIndex)++] = strdup(argv[optind]);
        optind++;
      } while (optind < argc && *argv[optind] != '-');
      break;

    case 7: // complete symbol replacement
      completeSymbolList[(*completeSymbolIndex)++] = strdup(optarg);
      break;
    case 'c':
      optind--;
      do {
        completeSymbolList[(*completeSymbolIndex)++] = strdup(argv[optind]);
        optind++;
      } while (optind < argc && *argv[optind] != '-');
      break;

    case 3:
      if (*singleStrPtr) {
        free(*singleStrPtr);
        printf("*** ***Only use the flag --singestr=<symbol> once.\n");
        exit(1);
      }
      *singleStrPtr = strdup(optarg);
      break;
    case 4:
      if (*keepNumStrPtr) {
        free(*keepNumStrPtr);
        printf("*** ***Only use the flag --keepnumstr=<symbol> once.\n");
        exit(1);
      }
      *keepNumStrPtr = strdup(optarg);
      break;
    case 8:
      if (*completeStrPtr) {
        free(*completeStrPtr);
        printf("*** ***Only use the flag --completestr=<symbol> once.\n");
        exit(1);
      }
      *completeStrPtr = strdup(optarg);
      break;

    case 9:
      if (def_or_undef == BOTH_DEF_AND_UNDEF) {
        def_or_undef = ONLY_DEF;
      } else {
        printf("*** ***Only use flag --def or --undef once.\n");
        exit(1);
      }
      break;
    case 10:
      if (def_or_undef == BOTH_DEF_AND_UNDEF) {
        def_or_undef = ONLY_UNDEF;
      } else {
        printf("*** ***Only use flag --def or --undef once.\n");
        exit(1);
      }
      break;

    case 'v':
      if (verbose != 0) {
        printf("*** ***Only use flag --verbose once.\n");
        exit(1);
      }
      verbose = 1;
      break;

    case '?':
      printf("\n*** invalid option given. (look above)\n\n");
      return -1;
      break;

    default:
      printf("?? getopt returned character code 0%o ??\n", c);
      return -1;
      break;
    }
  }

  if (optind < argc) {
    printf("non-option ARGV-elements: ");
    while (optind < argc)
      printf("%s ", argv[optind++]);
    printf("\n");
  }

  return 0;
}

void printObjectFileNames(int count, char *list[]) {
  if (debug_func)
    printf("printObjectFileNames\n");
  printf("The object files to modify are:\n");
  for (int i = 0; i < count; ++i)
    printf("(%d)\t%s\n", i, list[i]);
}

void printSymbolsToChange(int count, char *list[], char *str, FLAGTYPE ft) {
  if (debug_func)
    printf("printSymbolsToChange\n");
  switch (ft) {
  case SINGLESYM:
    printf("\n%s", "Single Symbols ");
    break;
  case KEEPNUMSYM:
    printf("\n%s", "Keep number Symbols ");
    break;
  case COMPLETESYM:
    printf("\n%s", "Complete Symbols ");
    break;
  default:
    printf("*** ***Unknown flagtype in printSymbolsToChange function.");
    exit(1);
  }
  printf("%s\n", "planned on changing:");
  for (int i = 0; i < count; ++i) {
    char buf[300] = {0};
    strcat(buf, list[i]);
    strcat(buf, " --> ");
    switch (ft) {
    case SINGLESYM:
      strcat(buf, list[i]);
      if (str) {
        strcat(buf, str);
      } else {
        strcat(buf, "__dmtcp_plt");
      }
      break;
    case KEEPNUMSYM:
      strcat(buf, list[i]);
      if (str) {
        strcat(buf, str);
        strcat(buf, "*");
      } else {
        strcat(buf, "__dmtcp_*");
      }
      break;
    case COMPLETESYM:
      strcat(buf, str);
      break;
    }
    printf("(%d)\t%s\n", i, buf);
  }
}

int checkAndFindElfFile(char *objFileName, Elf_Ehdr *ehdr) {
  if (debug_func)
    printf("checkAndFindElfFile\n");

  read_metadata(objFileName, (char *)ehdr, EI_NIDENT, 0);

  if (ehdr->ehdr.e_ident[EI_MAG0] != 0x7f &&
      ehdr->ehdr.e_ident[EI_MAG1] != 'E' && // CHECK IF ELF
      ehdr->ehdr.e_ident[EI_MAG2] != 'L' && ehdr->ehdr.e_ident[EI_MAG3] != 'F') {
    printf("Not an ELF executable\n");
    return -1;
  }
  ehdr->elfclass = ehdr->ehdr.e_ident[EI_CLASS];
  if (ehdr->ehdr.e_ident[EI_DATA] != ELFDATA2LSB) {
    printf("Not LSB Data\n");
    return -1;
  }
  // Now read the rest of the header based on ELFCLASS32 or ELFCLASS64
  switch (ehdr->elfclass) {
  case ELFCLASS32:
    read_metadata(objFileName, (char *)ehdr, sizeof(Elf32_Ehdr), 0);
    break;
  case ELFCLASS64:
    read_metadata(objFileName, (char *)ehdr, sizeof(Elf64_Ehdr), 0);
    break;
  default:
    printf("Unknown ELF Class\n");
    return -1;
  }
  // Note: the e_type has the same offset for 32 and 64 Elf Headers
  switch (ehdr->ehdr.elf64_ehdr.e_type) {
  case ET_EXEC:
    printf("WARNING: The ELf type is that of an executable. (%s)\n",
           objFileName);
    break;
  case ET_REL:
    printf("SUCCESS: The ELF type is that of an relocatable. (%s)\n",
           objFileName);
    break;
  default:
    printf("ERROR: The ELF type is that NOT of an EXEC nor REL. (%s)\n",
           objFileName);
    return -1;
  }
  return 0;
}

int calculateBytesNeeded(int index, char **list, char *str, FLAGTYPE ft) {
  if (debug_func)
    printf("calculateBytesNeeded\n");
  int result = 0;

  for (int i = 0; i < index; ++i) {
    char strNum[300] = {0};
    sprintf(strNum, "%d", i + 1);

    switch (ft) {
    case SINGLESYM:
      result += strlen(list[i]);
      if (str)
        result += strlen(str) + 1;
      else
        result += strlen("__dmtcp_plt") + 1;
      break;
    case KEEPNUMSYM:
      result += strlen(list[i]);
      if (str)
        result += strlen(strNum) + 1;
      else
        result += strlen("__dmtcp_") + strlen(strNum) + 1;
      break;
    case COMPLETESYM:
      if (str)
        result += strlen(str) + 1;
      break;
    }
  }

  // align on 64 bytes..
  result = (result + 63) & -64;

  return result;
}

#define ELF_N Elf64
#include "elfops.c"
#define ELF_N Elf32
#include "elfops.c"

// Main
int main(int argc, char **argv) {
  if (debug_func)
    printf("main\n");
  int objIndex = 0;
  char **objList = malloc(300 * sizeof(char *));

  int singleSymbolIndex = 0;
  char **singleSymbolList =
      malloc(300 * sizeof(char *)); // Symbols to add "__dmtcp_plt" to
  char *singleStr = NULL;

  int keepNumSymbolIndex = 0;
  char **keepNumSymbolList = malloc(
      300 * sizeof(char *)); // Symbols to add "__dmtcp_*" to, where * is 0 to n
  char *keepNumStr = NULL;

  int completeSymbolIndex = 0;
  char **completeSymbolList = malloc(
      300 * sizeof(char *)); // Symbols to compeltely replace with completeStr
  char *completeStr = NULL;

  // KJ Elf64_Shdr * shdr = NULL;
  // KJ Elf64_Shdr * symtab = NULL;
  // KJ Elf64_Shdr * strtab = NULL;
  // KJ unsigned long prev_strtab_size = 0;

  printf(
      "\n\n%s\n\n",
      "+++++++++++++++++++++++++++++++++ Started replace-symbols-name Program");

  if (argc < 4) {
    // perror("Syntax: ./change-symbol-names <obj file> symbol [othersymbols]");
    perror("Syntax: ./replace-symbols-name [-o | -s | --singlesymbol | "
           "--keepnumsymbol]");
    exit(1);
  }

  // Parse the arguments!
  assert(runGetOpt(argc, argv, &objIndex, objList, &singleSymbolIndex,
                   singleSymbolList, &keepNumSymbolIndex, keepNumSymbolList,
                   &completeSymbolIndex, completeSymbolList, &singleStr,
                   &keepNumStr, &completeStr) != -1);

  // Let user know which object files are going to change
  printObjectFileNames(objIndex, objList);

  // Let user know which symbols are going to change
  // assert(singleSymbolIndex + keepNumSymbolIndex > 0);
  if (singleSymbolIndex)
    printSymbolsToChange(singleSymbolIndex, singleSymbolList, singleStr,
                         SINGLESYM);
  if (keepNumSymbolIndex)
    printSymbolsToChange(keepNumSymbolIndex, keepNumSymbolList, keepNumStr,
                         KEEPNUMSYM);
  if (completeSymbolIndex)
    printSymbolsToChange(completeSymbolIndex, completeSymbolList, completeStr,
                         COMPLETESYM);
  printf("\n\n");

  for (int i = 0; i < objIndex; ++i) {
    unsigned long prev_strtab_size = 0;
    Elf_Ehdr ehdr;

    memset(actualSingleSymbolsToReplace, 0,
           sizeof(actualSingleSymbolsToReplace));
    memset(actualKeepNumSymbolsToReplace, 0,
           sizeof(actualKeepNumSymbolsToReplace));
    memset(actualCompleteSymbolsToReplace, 0,
           sizeof(actualCompleteSymbolsToReplace));
    actualSingleSymbolsIndex = 0;
    actualKeepNumSymbolsIndex = 0;
    actualCompleteSymbolsIndex = 0;

    // Check if file is valid and read in the ELF header
    assert(checkAndFindElfFile(objList[i], &ehdr) != -1);

    /************************************************************/
    if (ehdr.elfclass == ELFCLASS64) {
      // Find symbol table and string table
      //   - shdr = malloc(); <-- on heap
      Elf64_Shdr *shdr = NULL;
      Elf64_Shdr *symtab = NULL;
      Elf64_Shdr *strtab = NULL;
      assert(findSymtabAndStrtabAndShdr_Elf64(objList[i], ehdr.ehdr.elf64_ehdr,
					      &shdr, &symtab, &strtab) != -1);
      prev_strtab_size = strtab->sh_size;

      // Check if the symbols exist first..
      int symcount = 0;
      assert(checkIfSymbolsExist_Elf64(objList[i], singleSymbolIndex,
				       singleSymbolList,
				       keepNumSymbolIndex, keepNumSymbolList,
				       completeSymbolIndex, completeSymbolList,
				       symtab, strtab, &symcount) != -1);
      if (!symcount) {
	printf("        ^ ^ ^ continue\n\n");
	free(shdr);
	continue;
      }

      // Extend the string table and whatever follows after..
      // Fix Elf header and Section header Table
      int add_space = calculateBytesNeeded(actualSingleSymbolsIndex,
					   actualSingleSymbolsToReplace,
					   singleStr, SINGLESYM) +
                      calculateBytesNeeded(actualKeepNumSymbolsIndex,
					   actualKeepNumSymbolsToReplace,
					   keepNumStr, KEEPNUMSYM) +
                      calculateBytesNeeded(actualCompleteSymbolsIndex,
					   actualCompleteSymbolsToReplace,
					   completeStr, COMPLETESYM);
      if (debug)
	printf("ADD_SPACE is: %x\n", add_space);
      assert(extendAndFixAfterStrtab_Elf64(objList[i], &(ehdr.ehdr.elf64_ehdr),
					   &shdr, &symtab, &strtab,
					   add_space) != -1);

      // Add dmtcp symbol name(s) and update symtab
      assert(addSymbolsAndUpdateSymtab_Elf64(objList[i], i,
				     actualSingleSymbolsIndex,
				     actualSingleSymbolsToReplace, symtab,
                                     strtab, &prev_strtab_size, singleStr,
                                     SINGLESYM) != -1);
      assert(addSymbolsAndUpdateSymtab_Elf64(objList[i], i,
				     actualKeepNumSymbolsIndex,
                                     actualKeepNumSymbolsToReplace, symtab,
                                     strtab, &prev_strtab_size, keepNumStr,
                                     KEEPNUMSYM) != -1);
      assert(addSymbolsAndUpdateSymtab_Elf64(objList[i], i,
				     actualCompleteSymbolsIndex,
                                     actualCompleteSymbolsToReplace, symtab,
                                     strtab, &prev_strtab_size, completeStr,
                                     COMPLETESYM) != -1);

      for (int tmp_i = 0; tmp_i < actualSingleSymbolsIndex; ++tmp_i)
	free(actualSingleSymbolsToReplace[tmp_i]);
      for (int tmp_i = 0; tmp_i < actualKeepNumSymbolsIndex; ++tmp_i)
	free(actualKeepNumSymbolsToReplace[tmp_i]);
      for (int tmp_i = 0; tmp_i < actualCompleteSymbolsIndex; ++tmp_i)
	free(actualCompleteSymbolsToReplace[tmp_i]);
      free(shdr);


    /************************************************************/
    } else if (ehdr.elfclass == ELFCLASS32) {
      // Find symbol table and string table
      //   - shdr = malloc(); <-- on heap
      Elf32_Shdr *shdr = NULL;
      Elf32_Shdr *symtab = NULL;
      Elf32_Shdr *strtab = NULL;
      assert(findSymtabAndStrtabAndShdr_Elf32(objList[i], ehdr.ehdr.elf32_ehdr,
					      &shdr, &symtab, &strtab) != -1);
      prev_strtab_size = strtab->sh_size;

      // Check if the symbols exist first..
      int symcount = 0;
      assert(checkIfSymbolsExist_Elf32(objList[i], singleSymbolIndex,
				       singleSymbolList,
				       keepNumSymbolIndex, keepNumSymbolList,
				       completeSymbolIndex, completeSymbolList,
				       symtab, strtab, &symcount) != -1);
      if (!symcount) {
	printf("        ^ ^ ^ continue\n\n");
	free(shdr);
	continue;
      }

      // Extend the string table and whatever follows after..
      // Fix Elf header and Section header Table
      int add_space = calculateBytesNeeded(actualSingleSymbolsIndex,
					   actualSingleSymbolsToReplace,
					   singleStr, SINGLESYM) +
                      calculateBytesNeeded(actualKeepNumSymbolsIndex,
					   actualKeepNumSymbolsToReplace,
					   keepNumStr, KEEPNUMSYM) +
                      calculateBytesNeeded(actualCompleteSymbolsIndex,
					   actualCompleteSymbolsToReplace,
					   completeStr, COMPLETESYM);
      if (debug)
	printf("ADD_SPACE is: %x\n", add_space);
      assert(extendAndFixAfterStrtab_Elf32(objList[i], &(ehdr.ehdr.elf32_ehdr),
					   &shdr, &symtab, &strtab,
					   add_space) != -1);

      // Add dmtcp symbol name(s) and update symtab
      assert(addSymbolsAndUpdateSymtab_Elf32(objList[i], i,
				     actualSingleSymbolsIndex,
				     actualSingleSymbolsToReplace, symtab,
                                     strtab, &prev_strtab_size, singleStr,
                                     SINGLESYM) != -1);
      assert(addSymbolsAndUpdateSymtab_Elf32(objList[i], i,
				     actualKeepNumSymbolsIndex,
                                     actualKeepNumSymbolsToReplace, symtab,
                                     strtab, &prev_strtab_size, keepNumStr,
                                     KEEPNUMSYM) != -1);
      assert(addSymbolsAndUpdateSymtab_Elf32(objList[i], i,
				     actualCompleteSymbolsIndex,
                                     actualCompleteSymbolsToReplace, symtab,
                                     strtab, &prev_strtab_size, completeStr,
                                     COMPLETESYM) != -1);

      for (int tmp_i = 0; tmp_i < actualSingleSymbolsIndex; ++tmp_i)
	free(actualSingleSymbolsToReplace[tmp_i]);
      for (int tmp_i = 0; tmp_i < actualKeepNumSymbolsIndex; ++tmp_i)
	free(actualKeepNumSymbolsToReplace[tmp_i]);
      for (int tmp_i = 0; tmp_i < actualCompleteSymbolsIndex; ++tmp_i)
	free(actualCompleteSymbolsToReplace[tmp_i]);
      free(shdr);
    } else {
      printf("ERROR: Unknown ELF Class");
      exit(1);
    }
  }
  free(singleSymbolList);
  free(keepNumSymbolList);
  free(completeSymbolList);

  printf("\n\n%s\n\n", "Finished replace-symbols-name Program "
                       "+++++++++++++++++++++++++++++++++");
}
