#include <stdio.h>
#include <stdlib.h>
#include <mach-o/loader.h>

#ifdef __LP64__
typedef struct segment_command_64 segment_command_t;
typedef struct mach_header_64 mach_header_t;
typedef struct section_64 section_t;
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
#endif

void print_segment_command(segment_command_t* command);
void print_section(section_t section);

/// @brief Mach-O フィーマットの Load Command の内容を出力する
/// @param file_path 出力したいバイナリファイル（Mach-O フィーマット）
void display_mach_o_load_commands(const char* file_path)
{
    FILE* fp = fopen(file_path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to load a binary file [%s]\n", file_path);
        return;
    }

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    void* buffer = malloc(file_size);
    if (buffer == NULL)
    {
        fprintf(stderr, "Failed to allocate memory.\n");
        fclose(fp);
        return;
    }

    fread(buffer, 1, file_size, fp);

    const mach_header_t* header = (const mach_header_t*)buffer;
    if (header->magic != MH_MAGIC_64)
    {
        fprintf(stderr, "The file [%s] is not a Mach-O file.\n", file_path);
        free(buffer);
        fclose(fp);
        return;
    }

    printf("-------------------------------------\n");
    printf("Printing a mach-o header.\n");
    printf("-------------------------------------\n");


    printf("Magic: %d\n", header->magic);
    printf("CPU Type: %d\n", header->cputype);
    printf("CPU Sub Type: %d\n", header->cpusubtype);
    printf("File Type: %d\n", header->filetype);
    printf("Command Count: %d\n", header->ncmds);
    printf("Size of Commands: %d\n", header->sizeofcmds);
    printf("Flags: %d\n", header->flags);


    printf("-------------------------------------\n");
    printf("Printing mach-o segments.\n");
    printf("-------------------------------------\n");

    segment_command_t* cur_seg_cmd;

    // ヘッダの次の位置のポインタ = Load command の先頭位置
    uintptr_t cur = (uintptr_t)header + sizeof(mach_header_t);

    for (unsigned int i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize)
    {
        cur_seg_cmd = (segment_command_t*)cur;
        if (cur_seg_cmd->cmd == LC_SEGMENT_64)
        {
            print_segment_command(cur_seg_cmd);

            if (cur_seg_cmd->nsects == 0)
            {
                printf("-----------------------------------------\n");
                continue;
            }

            printf("- - - - - - - - - - - - - - - - -\n");

            // section 情報は segment 内にあり、segment 情報の直後に配置されている
            // そのため section_t 構造体は segment_command_t ひとつ分の
            // アドレスを進めた先（= cur_seg_cmd + 1）の位置から開始する
            section_t* section = (section_t*)(cur_seg_cmd + 1);
            for (uint32_t j = 0; j < cur_seg_cmd->nsects; j++)
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
            }
        }

        printf("-----------------------------------------\n");
    }

    free(buffer);
    fclose(fp);
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

void print_section(section_t section)
{

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