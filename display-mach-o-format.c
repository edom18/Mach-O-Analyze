#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __LP64__
typedef struct segment_command_64 segment_command_t;
typedef struct mach_header_64 mach_header_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#endif

#ifndef SEG_DATA_CONST
#define SEG_DATA_CONST  "__DATA_CONST"
#endif

void print_header(const mach_header_t* header);
void print_segment_command(segment_command_t* command);
void print_section(segment_command_t* seg_cmd);
void print_dysymtable(struct dysymtab_command* dysymtab_cmd);
void print_symtable(struct symtab_command* symtab_cmd);
void parse_symbol_table(uintptr_t base_addr, struct symtab_command* symtab_cmd, struct dysymtab_command* dysymtab_cmd);

/// @brief Mach-O フィーマットの Load Command の内容を出力する
/// @param file_path 出力したいバイナリファイル（Mach-O フィーマット）
void display_mach_o_load_commands(const char* file_path)
{
    int fd = open(file_path, O_RDONLY);

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        perror("fstat");
        close(fd);
        return;
    }

    void* buffer = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buffer == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return;
    }
    close(fd);

    const mach_header_t* header = (const mach_header_t*)buffer;
    if (header->magic != MH_MAGIC_64)
    {
        fprintf(stderr, "The file [%s] is not a Mach-O file.\n", file_path);
        munmap(buffer, st.st_size);
        return;
    }

    // ----------------------------------

    print_header(header);

    segment_command_t* cur_seg_cmd;
    struct symtab_command* symtab_cmd;
    struct dysymtab_command* dysymtab_cmd;

    // ヘッダの次の位置のポインタ = Load command の先頭位置
    uintptr_t cur = (uintptr_t)header + sizeof(mach_header_t);

    // 必要なセグメントを検索する
    for (unsigned int i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize)
    {
        cur_seg_cmd = (segment_command_t*)cur;
        if (cur_seg_cmd->cmd == LC_SYMTAB)
        {
            symtab_cmd = (struct symtab_command*)cur_seg_cmd;
        }
        else if (cur_seg_cmd->cmd == LC_DYSYMTAB)
        {
            dysymtab_cmd = (struct dysymtab_command*)cur_seg_cmd;
        }
    }

    // Symbol table と Dynamic symbol table が合った場合だけ出力
    if (symtab_cmd && dysymtab_cmd)
    {
        uintptr_t base_addr = (uintptr_t)buffer;
        parse_symbol_table(base_addr, symtab_cmd, dysymtab_cmd);
    }

    printf("-------------------------------------\n");
    printf("Printing mach-o segments.\n");
    printf("-------------------------------------\n");

    cur = (uintptr_t)header + sizeof(mach_header_t);
    uintptr_t base_addr = (uintptr_t)buffer;
    nlist_t* symtab = (nlist_t*)(base_addr + symtab_cmd->symoff);
    char* strtab = (char*)(base_addr + symtab_cmd->stroff);
    for (unsigned int i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize)
    {
        cur_seg_cmd = (segment_command_t*)cur;
        if (cur_seg_cmd->cmd != LC_SEGMENT_64)
        {
            continue;
        }

        print_segment_command(cur_seg_cmd);

        if (cur_seg_cmd->nsects == 0)
        {
            printf("==================================\n");
            continue;
        }

        printf("------------------- Sections\n");

        print_section(cur_seg_cmd);

        if (strcmp(cur_seg_cmd->segname, SEG_DATA) != 0 &&
            strcmp(cur_seg_cmd->segname, SEG_DATA_CONST) != 0)
        {
            continue;
        }

        printf("--------- Dynamic symbols\n");
        for (uint32_t j = 0; j < cur_seg_cmd->nsects; j++)
        {
            section_t* section = (section_t*)(cur + sizeof(segment_command_t)) + j;
            if ((section->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
                (section->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS)
            {

                uint32_t* indirect_symtab = (uint32_t*)(base_addr + dysymtab_cmd->indirectsymoff);
                uint32_t* indirect_symbol_indices = indirect_symtab + section->reserved1;
                void** indirect_symbol_bindings = (void**)((uintptr_t)base_addr + section->addr);

                for (uint32_t k = 0; k < section->size / sizeof(void*); k++)
                {
                    uint32_t symtab_index = indirect_symbol_indices[k];
                    if (symtab_index == INDIRECT_SYMBOL_ABS || symtab_index == INDIRECT_SYMBOL_LOCAL ||
                        symtab_index == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS))
                    {
                        continue;
                    }

                    uint32_t strtab_offset = symtab[symtab_index].n_un.n_strx;
                    char* symbol_name = strtab + strtab_offset;

                    printf("  - %s\n", symbol_name);
                }
            }
        }

        printf("----------------------------------\n");
    }

    munmap(buffer, st.st_size);
}

void print_header(const mach_header_t* header)
{
    printf("-----------------------------------------------------------------------\n");
    printf("Printing a mach-o header.\n");
    printf("-----------------------------------------------------------------------\n");
    printf("Magic: %d\n", header->magic);
    printf("CPU Type: %d\n", header->cputype);
    printf("CPU Sub Type: %d\n", header->cpusubtype);
    printf("File Type: %d\n", header->filetype);
    printf("Command Count: %d\n", header->ncmds);
    printf("Size of Commands: %d\n", header->sizeofcmds);
    printf("Flags: %d\n", header->flags);
    printf("\n\n");
}

void print_segment_command(segment_command_t* command)
{
    if (command == NULL)
    {
        return;
    }

    printf("     cmd: LC_SEGMENT_64\n");
    printf(" cmdsize: %d\n", command->cmdsize);
    printf(" segname: %s\n", command->segname);
    printf("  vmaddr: 0x%016llx\n", command->vmaddr);
    printf("  vmsize: 0x%016llx\n", command->vmsize);
    printf(" fileoff: %llu\n", command->fileoff);
    printf("filesize: %llu\n", command->filesize);
    printf(" maxprot: 0x%08x\n", command->maxprot);
    printf("initprot: 0x%08x\n", command->initprot);
    printf("  nsects: %d\n", command->nsects);
    printf("   flags: 0x%d\n", command->flags);
}

void print_section(segment_command_t* seg_cmd)
{
    // section 情報は segment 内にあり、segment 情報の直後に配置されている
    // そのため section_t 構造体は segment_command_t ひとつ分の
    // アドレスを進めた先（= cur_seg_cmd + 1）の位置から開始する
    section_t* section = (section_t*)(seg_cmd + 1);
    for (uint32_t j = 0; j < seg_cmd->nsects; j++)
    {
        printf(" sectname: %s\n", section[j].sectname);
        printf("  segname: %s\n", section[j].segname);
        printf("     addr: 0x%016llx\n", section[j].addr);
        printf("     size: 0x%016llx\n", section[j].size);
        printf("   offset: %d\n", section[j].offset);
        printf("    align: %d\n", section[j].align);
        printf("   reloff: %d\n", section[j].reloff);
        printf("   nreloc: %d\n", section[j].nreloc);
        printf("    flags: 0x%08x\n", section[j].flags);
        printf("reserved1: %d\n", section[j].reserved1);
        printf("reserved2: %d\n", section[j].reserved2);
        printf("reserved3: %d\n", section[j].reserved3);
        printf("- - - - - - - - - - - - - - - - - - - - - - \n");
    }
}

void print_dysymtable(struct dysymtab_command* dysymtab_cmd)
{
    printf("[Dynamic symbol table]\n");
    printf("           cmd: %d\n", dysymtab_cmd->cmd);
    printf("       cmdsize: %d\n", dysymtab_cmd->cmdsize);
    printf("     ilocalsym: %d\n", dysymtab_cmd->ilocalsym);
    printf("     nlocalsym: %d\n", dysymtab_cmd->nlocalsym);
    printf("    iextdefsym: %d\n", dysymtab_cmd->iextdefsym);
    printf("    nextdefsym: %d\n", dysymtab_cmd->nextdefsym);
    printf("        tocoff: %d\n", dysymtab_cmd->tocoff);
    printf("          ntoc: %d\n", dysymtab_cmd->ntoc);
    printf("     modtaboff: %d\n", dysymtab_cmd->modtaboff);
    printf("       nmodtab: %d\n", dysymtab_cmd->nmodtab);
    printf("  extrefsymoff: %d\n", dysymtab_cmd->extrefsymoff);
    printf("   nextrefsyms: %d\n", dysymtab_cmd->nextrefsyms);
    printf("indirectsymoff: %d\n", dysymtab_cmd->indirectsymoff);
    printf(" nindirectsyms: %d\n", dysymtab_cmd->nindirectsyms);
    printf("     extreloff: %d\n", dysymtab_cmd->extreloff);
    printf("       nextrel: %d\n", dysymtab_cmd->nextrel);
    printf("     locreloff: %d\n", dysymtab_cmd->locreloff);
    printf("       nlocrel: %d\n", dysymtab_cmd->nlocrel);
    printf("\n\n");
}

void print_symtable(struct symtab_command* symtab_cmd)
{
    printf("[Symbol table]\n");
    printf("    cmd: %d\n", symtab_cmd->cmd);
    printf("cmdsize: %d\n", symtab_cmd->cmdsize);
    printf(" symoff: %d\n", symtab_cmd->symoff);
    printf("  nsyms: %d\n", symtab_cmd->nsyms);
    printf(" stroff: %d\n", symtab_cmd->stroff);
    printf("strsize: %d\n", symtab_cmd->strsize);
    printf("\n\n");
}

// [Mach-O ファイル構造]
// 
// ファイル先頭 (offset 0)
//   |
//   |---- ... 他セグメント ...
//   |
//   |---- __LINKEDIT セグメント開始 (offset = linkedit_segment->fileoff)
//   |       \
//   |        +--- symoff で指定されるシンボルテーブル相対位置
//   |        +--- stroff で指定される文字列表相対位置
//   |
//   +---- ファイル終端
void parse_symbol_table(uintptr_t base_addr, struct symtab_command* symtab_cmd, struct dysymtab_command* dysymtab_cmd)
{
    printf("-----------------------------------------------------------------------\n");
    printf("Printing a symbol table and a dynamic symbol table metadata\n");
    printf("-----------------------------------------------------------------------\n");

    print_symtable(symtab_cmd);
    print_dysymtable(dysymtab_cmd);

    printf("-----------------------------------------------------------------------\n");
    printf("Printing a symbol table and a dynamic symbol table\n");
    printf("-----------------------------------------------------------------------\n");

    // stroff はファイル先頭からのオフセット
    uint32_t stroff = symtab_cmd->stroff;
    uint32_t strsize = symtab_cmd->strsize;

    // string table 内は \0 区切りの文字列配列
    // インデックス 0 は空文字列。1 つめの実シンボル名は strtab + nlist.n_un.n_strx で参照される
    char* strtab = (char*)base_addr + stroff;
    nlist_t* symtab = (nlist_t*)(base_addr + symtab_cmd->symoff);

    // struct nlist_64 {
    //     union {
    //         uint32_t  n_strx; /* index into the string table */
    //     } n_un;
    //     uint8_t n_type;        /* type flag, see below */
    //     uint8_t n_sect;        /* section number or NO_SECT */
    //     uint16_t n_desc;       /* see <mach-o/stab.h> */
    //     uint64_t n_value;      /* value of this symbol (or stab offset) */
    // };
    for (uint32_t i = 0; i < symtab_cmd->nsyms; i++)
    {
        uint32_t strtab_offset = symtab[i].n_un.n_strx;
        char* symbol_name = strtab + strtab_offset;
        printf("Symbol name: %s\n", symbol_name);
    }

    printf("\n\n");
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <binary_file>\n", argv[0]);
        return 1;
    }

    const char* file_path = argv[1];
    display_mach_o_load_commands(file_path);

    return 0;
}