#include <stdio.h>
#include <stdlib.h>
#include <mach-o/loader.h>

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

    printf("-------------------------------------\n");
    printf("Printing a mach-o header.\n");
    printf("-------------------------------------\n");

    const struct mach_header_64* header = (const struct mach_header_64*)buffer;

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

    struct segment_command_64* cur_seg_cmd;

    // ヘッダの次の位置のポインタ = Load command の先頭位置
    uintptr_t cur = (uintptr_t)header + sizeof(struct mach_header_64);

    for (unsigned int i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize)
    {
        cur_seg_cmd = (struct segment_command_64*)cur;
        if (cur_seg_cmd->cmd == LC_SEGMENT_64)
        {
            printf("     cmd: LC_SEGMENT_64\n");
            printf(" cmdsize: %d\n", cur_seg_cmd->cmdsize);
            printf(" segname: [%s]\n", cur_seg_cmd->segname);
            printf("  vmaddr: 0x%016llx\n", cur_seg_cmd->vmaddr);
            printf("  vmsize: 0x%016llx\n", cur_seg_cmd->vmsize);
            printf(" fileoff: %llu\n", cur_seg_cmd->fileoff);
            printf("filesize: %llu\n", cur_seg_cmd->filesize);
            printf(" maxprot: %d\n", cur_seg_cmd->maxprot);
            printf("initprot: %d\n", cur_seg_cmd->initprot);
            printf("  nsects: %d\n", cur_seg_cmd->nsects);
            printf("   flags: 0x%d\n", cur_seg_cmd->flags);
        }

        printf("-----------------------------------------\n");
    }

    free(buffer);
    fclose(fp);
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