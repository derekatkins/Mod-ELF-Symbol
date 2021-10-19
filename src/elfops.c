#define MK_FCN_NAME(name, vers) name ## vers
#define FUNCTION_NAME(name, vers) MK_FCN_NAME(name, vers)

#define MK_TYPE_NAME(name, vers) vers ## name
#define TYPE_NAME(name, vers) MK_TYPE_NAME(name, vers)
#define ElfType_Ehdr TYPE_NAME(_Ehdr, ELF_N)
#define ElfType_Shdr TYPE_NAME(_Shdr, ELF_N)
#define ElfType_Sym  TYPE_NAME(_Sym,  ELF_N)
#define ElfType_Off  TYPE_NAME(_Off,  ELF_N)

int FUNCTION_NAME(findSymtabAndStrtabAndShdr_, ELF_N)(char *objFileName, ElfType_Ehdr ehdr,
                               ElfType_Shdr **shdr, ElfType_Shdr **symtab,
                               ElfType_Shdr **strtab) {
  if (debug_func)
    printf("%s\n", __FUNCTION__);
  unsigned long shoff = ehdr.e_shoff; // Section header table offset
  unsigned long shnum = ehdr.e_shnum; // Section header num entries
  *shdr = malloc(sizeof(ElfType_Shdr) *
                 ehdr.e_shnum); // Copy of section header table entries
  read_metadata(objFileName, (char *)*shdr, (shnum) * sizeof(ElfType_Shdr),
                shoff); // read in section headers

  ElfType_Shdr *hdr = *shdr;
  int idx;

  // Go through the section table entries
  for (idx = 0; idx < shnum; idx++, hdr++) {
    switch (hdr->sh_type) {
    case SHT_SYMTAB:
      *symtab = hdr;
      if (debug)
        printf("symtab: size=%lu offset=%p\n", hdr->sh_size,
               (void *)hdr->sh_offset);
      *strtab = *shdr + hdr->sh_link;
      if (debug)
        printf("strtab: size=%lu offset=%p\n", (*strtab)->sh_size,
               (void *)(*strtab)->sh_offset);
      assert((*strtab)->sh_type == SHT_STRTAB);
      break;
    }
  }
  if (*symtab == NULL || *strtab == NULL)
    return -1;
  if (debug)
    printf("%p %p %s\n", *symtab, *strtab,
           "Found Symbol table and String Table");
  return 0;
}

int FUNCTION_NAME(loopSymbolTableForSymbol_, ELF_N)(int index, char **list, ElfType_Sym *symtab_ent,
                             unsigned int symtab_size, char *strtab_ent,
                             FLAGTYPE ft, int *symcountptr) {
  if (debug_func)
    printf("%s\n", __FUNCTION__);
  if (!index)
    return 0;
  ElfType_Sym *sym = NULL;

  if (ft == SINGLESYM)
    printf("\t%s\n", "Single Symbols");
  if (ft == KEEPNUMSYM)
    printf("\t%s\n", "Keep Number Symbols");
  if (ft == COMPLETESYM)
    printf("\t%s\n", "Complete Symbols");

  for (int i = 0; i < index; ++i) {
    int found = 0;
    for (sym = symtab_ent; (char *)sym < (char *)symtab_ent + symtab_size;
         sym++) {
      if (sym->st_name != 0) {
        char *symtab_symbol = strtab_ent + sym->st_name;
        if (strcmp(symtab_symbol, list[i]) == 0) {
          if ((def_or_undef == ONLY_DEF && sym->st_shndx == SHN_UNDEF) ||
              (def_or_undef == ONLY_UNDEF && sym->st_shndx != SHN_UNDEF)) {
            if (verbose)
              printf("\t\tContinue because of def or undef\n");
            continue;
          }

          printf("\t\tsymbol: %s  |  ", strtab_ent + sym->st_name);
          printf("sym->st_value: %p\n", (void *)sym->st_value);
          found = 1;

          // Copy into correct (single | keepnum | complete) syms to replace
          // array
          switch (ft) {
          case SINGLESYM:
            actualSingleSymbolsToReplace[actualSingleSymbolsIndex++] =
                strdup(symtab_symbol);
            break;
          case KEEPNUMSYM:
            actualKeepNumSymbolsToReplace[actualKeepNumSymbolsIndex++] =
                strdup(symtab_symbol);
            break;
          case COMPLETESYM:
            actualCompleteSymbolsToReplace[actualCompleteSymbolsIndex++] =
                strdup(symtab_symbol);
            break;
          }
        }
      }
    }
    if (!found) {
      if (verbose)
        printf("\t\t*** ***Could not find: %s\n", list[i]);
      if (ft == KEEPNUMSYM) {
        printf("\t\tKeep Number Symbol : (%s) was NOT FOUND. [ERROR]\n",
               list[i]);
        return -1;
      }
    } else
      ++(*symcountptr);
  }
  if (verbose)
    printf("\t\tNumber of symbols to replace is: %d\n", *symcountptr);
  return 0;
}

int FUNCTION_NAME(checkIfSymbolsExist_, ELF_N)(char *objFileName, int singleSymbolIndex,
                        char **singleSymbolList, int keepNumSymbolIndex,
                        char **keepNumSymbolList, int completeSymbolIndex,
                        char **completeSymbolList, ElfType_Shdr *symtab,
                        ElfType_Shdr *strtab, int *symcountptr) {
  if (debug_func)
    printf("%s\n", __FUNCTION__);
  ElfType_Sym *sym = NULL;
  ElfType_Sym *symtab_ent = NULL;
  char *strtab_ent = NULL;

  symtab_ent = malloc(symtab->sh_size);
  if (debug)
    printf("symtab: size=%lu offset=%p\n", symtab->sh_size,
           (void *)symtab->sh_offset);
  read_metadata(objFileName, (char *)symtab_ent, symtab->sh_size,
                symtab->sh_offset);
  strtab_ent = malloc(strtab->sh_size);
  if (debug)
    printf("strtab: size=%lu offset=%p\n", strtab->sh_size,
           (void *)strtab->sh_offset);
  read_metadata(objFileName, strtab_ent, strtab->sh_size, strtab->sh_offset);

  unsigned int symtab_size = symtab->sh_size;

  printf("In file %s symbols checked:\n", objFileName);

  // Loop for Single Sym
  FUNCTION_NAME(loopSymbolTableForSymbol_, ELF_N)(singleSymbolIndex, singleSymbolList, symtab_ent,
                           symtab_size, strtab_ent, SINGLESYM, symcountptr);

  // Loop for Keep Num Sym
  if (FUNCTION_NAME(loopSymbolTableForSymbol_, ELF_N)(keepNumSymbolIndex, keepNumSymbolList,
                               symtab_ent, symtab_size, strtab_ent, KEEPNUMSYM,
                               symcountptr) == -1) {
    return -1;
  }

  // Loop for Complete Sym
  FUNCTION_NAME(loopSymbolTableForSymbol_, ELF_N)(completeSymbolIndex, completeSymbolList, symtab_ent,
                           symtab_size, strtab_ent, COMPLETESYM, symcountptr);
  return 0;
}

int FUNCTION_NAME(extendAndFixAfterStrtab_, ELF_N)(char *objFileName, ElfType_Ehdr *ehdr,
                            ElfType_Shdr **shdr, ElfType_Shdr **symtab,
                            ElfType_Shdr **strtab, int add_space) {
  if (debug_func)
    printf("%s\n", __FUNCTION__);
  ElfType_Off begin_next_section = (*strtab)->sh_offset + (*strtab)->sh_size;
  ElfType_Off end_of_sections = (*strtab)->sh_offset + (*strtab)->sh_size;

  ehdr->e_shoff += add_space; // We will be displacing section header table
  (*strtab)->sh_size += add_space;

  if (debug)
    printf("bns:%p  eos:%p\n", (void *)begin_next_section,
           (void *)end_of_sections);

  // Modify Section Header Table and write back
  int idx = 0;
  ElfType_Shdr *ptr = *shdr;
  for (; idx < ehdr->e_shnum; ++idx, ++ptr) {
    ElfType_Shdr tmpshdr = *ptr;
    if (tmpshdr.sh_offset > (*strtab)->sh_offset) {
      ptr->sh_offset += add_space;
    }
    if (tmpshdr.sh_offset + tmpshdr.sh_size >
        end_of_sections) { // End of section header
      end_of_sections = tmpshdr.sh_offset + tmpshdr.sh_size;
    }
    if (debug)
      printf("bns:%p  eos:%p ptr:%p\n", (void *)begin_next_section,
             (void *)end_of_sections, ptr);
  }
  if (debug)
    printf("bns:%p  eos:%p\n", (void *)begin_next_section,
           (void *)end_of_sections);

  // Displace all sections after strtab by 'add_space'.
  assert(end_of_sections <= ehdr->e_shoff);
  if (end_of_sections > begin_next_section) {
    if (debug)
      printf("start of if: %s\n",  __FUNCTION__);
    char tmpbuf[end_of_sections - begin_next_section];
    read_metadata(objFileName, tmpbuf, sizeof(tmpbuf), begin_next_section);
    // read_metadata(objFileName, tmpbuf, end_of_sections - begin_next_section,
    // begin_next_section);
    if (debug)
      printf("middle of if: between read_metadata and write_metadata %s\n",
	     __FUNCTION__);
    write_metadata(objFileName, tmpbuf, sizeof(tmpbuf),
                   begin_next_section + add_space);
    if (debug)
      printf("end of if: %s\n", __FUNCTION__);
  }

  // Write back section headers and ELF header
  write_metadata(objFileName, (char *)*shdr,
                 (ehdr->e_shnum) * sizeof(ElfType_Shdr), ehdr->e_shoff);
  write_metadata(objFileName, (char *)ehdr, sizeof(ElfType_Ehdr), 0);

  return 0;
}

int FUNCTION_NAME(addSymbolsAndUpdateSymtab_, ELF_N)(char *objFileName, int num, int index,
                              char **list, ElfType_Shdr *symtab,
                              ElfType_Shdr *strtab,
                              unsigned long *prev_strtab_size, char *str,
                              FLAGTYPE ft) {
  if (debug_func)
    printf("%s\n", __FUNCTION__);
  if (index < 1)
    return 0;
  // From previous function checkIfSymbolsExist_Elf<N>(), we know all the symbols exist

  int numSymbolsToChange = index;
  unsigned int symtab_size = symtab->sh_size;

  ElfType_Sym *sym = NULL;
  char symtab_buf[symtab->sh_size];
  char strtab_buf[strtab->sh_size];
  char numbuf[300] = {0};
  memset(numbuf, 0, sizeof(numbuf));
  sprintf(numbuf, "%d", num + 1);

  read_metadata(objFileName, symtab_buf, symtab->sh_size, symtab->sh_offset);
  read_metadata(objFileName, strtab_buf, strtab->sh_size, strtab->sh_offset);

  for (int i = 0; i < numSymbolsToChange; ++i) {
    for (sym = (void *)symtab_buf;
         (char *)sym < (char *)symtab_buf + symtab->sh_size; sym++) {
      if (sym->st_name != 0) {

        char *symtab_symbol = strtab_buf + sym->st_name;

        if (strcmp(symtab_symbol, list[i]) == 0) {
          int newStringSize = 0;
          char newString[300] = {0};

          memset(newString, 0, sizeof(newString));

          sym->st_name = *prev_strtab_size;

          switch (ft) {
          case SINGLESYM:
            strcat(newString, list[i]);
            if (str) {
              strcat(newString, str);
            } else {
              strcat(newString, "__dmtcp_plt");
            }
            break;
          case KEEPNUMSYM:
            strcat(newString, list[i]);
            if (str) {
              strcat(newString, str);
            } else {
              strcat(newString, "__dmtcp_");
            }
            strcat(newString, numbuf);
            break;
          case COMPLETESYM:
            strcat(newString, str);
            break;
          default:
            break;
          }
          strcat(newString, "\0");
          strncpy((char *)strtab_buf + *prev_strtab_size, newString,
                  strlen(newString) + 1);
          *prev_strtab_size += strlen(newString) + 1;

          if (debug)
            printf("NEW STRING IS **** %s ****\n", newString);
        }
      }
    }
    if (debug)
      printf("strtab->sh_size = %lu : prev = %lu\n", strtab->sh_size,
             *prev_strtab_size);
  }

  // Pad out with '\0'
  for (int i = *prev_strtab_size; i < strtab->sh_size; ++i)
    strtab_buf[i] = '\0';

  write_metadata(objFileName, (char *)symtab_buf, symtab->sh_size,
                 symtab->sh_offset);
  write_metadata(objFileName, (char *)strtab_buf, strtab->sh_size,
                 strtab->sh_offset);

  return 0;
}
