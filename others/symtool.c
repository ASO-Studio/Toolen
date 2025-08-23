#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <ctype.h>

#include "config.h"
#include "module.h"

#define ELF_ERROR(msg) do { fprintf(stderr, "ELF Error: %s\n", msg); exit(1); } while(0)
#define FILE_ERROR(fmt, ...) do { \
	fprintf(stderr, "File Error: "); \
	fprintf(stderr, fmt, ##__VA_ARGS__); \
	fprintf(stderr, "\n"); \
	exit(1); \
} while(0)

//  Elf file type
typedef struct {
	void *data;		// Address
	size_t size;		// File size
	Elf64_Ehdr *ehdr;	// ELF header
	Elf64_Shdr *shdr;	// Section header
	char *shstrtab;		// Section string table
	Elf64_Shdr *symtab;	// Symbols table
	Elf64_Shdr *strtab;	// String table
} ElfFile;

// Symbol location information
typedef struct {
	char *name;		// Symbol name
	Elf64_Addr vaddr;	// Virtual address
	Elf64_Off offset;	// File offset
	size_t size;		// Symbol size
	Elf64_Shdr *section;	// Section
	unsigned char type;	// Symbol type
	unsigned char bind;	// Bind type
} SymbolLocation;

// Load elf file
static ElfFile load_elf(const char *filename, int writable) {
	ElfFile elf = {0};

	int flags = writable ? O_RDWR : O_RDONLY;
	int fd = open(filename, flags);
	if (fd < 0) {
		FILE_ERROR("Failed to open '%s': %s", filename, strerror(errno));
	}
	
	// Get file size
	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		FILE_ERROR("fstat failed for '%s': %s", filename, strerror(errno));
	}
	
	if (st.st_size == 0) {
		close(fd);
		FILE_ERROR("File '%s' is empty", filename);
	}
	
	elf.size = st.st_size;

	int prot = PROT_READ | (writable ? PROT_WRITE : 0);
	elf.data = mmap(NULL, elf.size, prot, MAP_SHARED, fd, 0);
	if (elf.data == MAP_FAILED) {
		close(fd);
		FILE_ERROR("mmap failed for '%s': %s", filename, strerror(errno));
	}
	
	close(fd);
	
	// Parse elf header
	elf.ehdr = (Elf64_Ehdr *)elf.data;
	
	// Check
	if (memcmp(elf.ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
		munmap(elf.data, elf.size);
		FILE_ERROR("'%s' is not a valid ELF file", filename);
	}
	
	// Check file size
	if (elf.ehdr->e_shoff + (elf.ehdr->e_shnum * elf.ehdr->e_shentsize) > elf.size) {
		munmap(elf.data, elf.size);
		FILE_ERROR("'%s' is corrupted or truncated", filename);
	}
	
	// Get section header
	elf.shdr = (Elf64_Shdr *)((char *)elf.data + elf.ehdr->e_shoff);
	
	// Get section string table
	if (elf.ehdr->e_shstrndx != SHN_UNDEF) {
		if (elf.ehdr->e_shstrndx >= elf.ehdr->e_shnum) {
			munmap(elf.data, elf.size);
			FILE_ERROR("Invalid section header string table index in '%s'", filename);
		}
		
		Elf64_Shdr *shstrtab_hdr = &elf.shdr[elf.ehdr->e_shstrndx];
		if (shstrtab_hdr->sh_offset + shstrtab_hdr->sh_size > elf.size) {
			munmap(elf.data, elf.size);
			FILE_ERROR("Section header string table out of bounds in '%s'", filename);
		}
		
		elf.shstrtab = (char *)elf.data + shstrtab_hdr->sh_offset;
	}

	for (int i = 0; i < elf.ehdr->e_shnum; i++) {
		if (elf.shdr[i].sh_offset + elf.shdr[i].sh_size > elf.size) {
			// Skip invalid section
			continue;
		}
		
		const char *section_name = elf.shstrtab + elf.shdr[i].sh_name;

		if (elf.shdr[i].sh_type == SHT_SYMTAB) {
			elf.symtab = &elf.shdr[i];
		} else if (strcmp(section_name, ".strtab") == 0) {
			elf.strtab = &elf.shdr[i];
		}
	}
	
	if (!elf.symtab) {
		munmap(elf.data, elf.size);
		FILE_ERROR("Symbol table not found in '%s'", filename);
	}
	
	if (!elf.strtab) {
		munmap(elf.data, elf.size);
		FILE_ERROR("String table not found in '%s'", filename);
	}
	
	return elf;
}

static void free_elf(ElfFile *elf) {
	if (elf->data) {
		munmap(elf->data, elf->size);
		elf->data = NULL;
	}
}

// Search symbol location
static SymbolLocation find_symbol_location(ElfFile *elf, const char *symbol_name) {
	SymbolLocation loc = {0};
	Elf64_Sym *symtab = (Elf64_Sym *)((char *)elf->data + elf->symtab->sh_offset);
	char *strtab = (char *)elf->data + elf->strtab->sh_offset;
	int num_symbols = elf->symtab->sh_size / sizeof(Elf64_Sym);
	
	// Search symbol
	for (int i = 0; i < num_symbols; i++) {
		Elf64_Sym *sym = &symtab[i];
		const char *name = strtab + sym->st_name;
		
		if (strcmp(name, symbol_name) == 0) {
			loc.name = (char*)name;
			loc.vaddr = sym->st_value;
			loc.size = sym->st_size;
			loc.type = ELF64_ST_TYPE(sym->st_info);
			loc.bind = ELF64_ST_BIND(sym->st_info);
			
			// Skip undefined symbol
			if (sym->st_shndx == SHN_UNDEF) {
				FILE_ERROR("Symbol '%s' is undefined", symbol_name);
			}

			if (sym->st_shndx < elf->ehdr->e_shnum) {
				loc.section = &elf->shdr[sym->st_shndx];

				// Get offset
				if (loc.section->sh_type != SHT_NOBITS) {
					loc.offset = loc.vaddr - loc.section->sh_addr + loc.section->sh_offset;

					// Check
					if (loc.offset < loc.section->sh_offset || 
						loc.offset + loc.size > loc.section->sh_offset + loc.section->sh_size) {
						FILE_ERROR("Symbol '%s' is out of section bounds", symbol_name);
					}
					
					return loc;
				} else {
					FILE_ERROR("Symbol '%s' is in a NOBITS section (no file content)", symbol_name);
				}
			} else {
				FILE_ERROR("Invalid section index for symbol '%s'", symbol_name);
			}
		}
	}
	
	FILE_ERROR("Symbol '%s' not found", symbol_name);
	return loc; // unreachable
}

// Extract symbol context
static void extract_symbol_binary(ElfFile *elf, const char *symbol_name, const char *output_file) {
	SymbolLocation loc = find_symbol_location(elf, symbol_name);

	FILE *fp = fopen(output_file, "wb");
	if (!fp) {
		FILE_ERROR("Failed to create output file '%s': %s", output_file, strerror(errno));
	}

	void *symbol_data = (char *)elf->data + loc.offset;
	if (fwrite(symbol_data, loc.size, 1, fp) != 1) {
		fclose(fp);
		FILE_ERROR("Failed to write symbol data to '%s': %s", output_file, strerror(errno));
	}
	
	fclose(fp);
	printf("Successfully extracted %zu bytes of symbol '%s' to '%s'\n", 
		   loc.size, symbol_name, output_file);
}

// Write back symbol context
static void restore_symbol_binary(ElfFile *elf, const char *symbol_name, const char *input_file) {
	SymbolLocation loc = find_symbol_location(elf, symbol_name);

	FILE *fp = fopen(input_file, "rb");
	if (!fp) {
		FILE_ERROR("Failed to open input file '%s': %s", input_file, strerror(errno));
	}

	fseek(fp, 0, SEEK_END);
	size_t file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	// Check file size
	if (file_size != loc.size) {
		fclose(fp);
		FILE_ERROR("Input file size (%zu) does not match symbol size (%zu)", file_size, loc.size);
	}
	
	// Read binary data
	void *new_data = malloc(loc.size);
	if (!new_data) {
		fclose(fp);
		FILE_ERROR("Memory allocation failed");
	}
	
	if (fread(new_data, loc.size, 1, fp) != 1) {
		free(new_data);
		fclose(fp);
		FILE_ERROR("Failed to read symbol data from '%s'", input_file);
	}
	
	fclose(fp);
	
	// Write back
	void *symbol_data = (char *)elf->data + loc.offset;
	memcpy(symbol_data, new_data, loc.size);
	
	free(new_data);
	printf("Successfully restored %zu bytes to symbol '%s'\n", loc.size, symbol_name);
}

// Print help informations
static void symtool_show_help() {
	SHOW_VERSION(stdout);
	printf("Usage: symtool <command> [options] <elf_file>\n");
	printf("\nCommands:\n");
	printf("  extract             - Extract symbol binary data\n");
	printf("  restore             - Restore symbol binary data\n");
	printf("\nOptions:\n");
	printf("  -s, --symbol <name>   Symbol name (for extract/restore)\n");
	printf("  -o, --output <file>   Output file (for extract)\n");
	printf("  -i, --input <file>    Input file (for restore)\n");
	printf("\nExamples:\n");
	printf("  symtool extract -s main -o main.bin program.elf\n");
	printf("  symtool restore -s main -i new_main.bin program.elf\n");
}

int symtool_main(int argc, char *argv[]) {
	if (argc < 2) {
		symtool_show_help();
		return 1;
	}
	
	const char *command = NULL;
	const char *elf_file = NULL;
	const char *symbol_name = NULL;
	const char *output_file = NULL;
	const char *input_file = NULL;

	int opt;
	int option_index = 0;
	static struct option long_options[] = {
		{"symbol", required_argument, 0, 's'},
		{"output", required_argument, 0, 'o'},
		{"input", required_argument, 0, 'i'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	command = argv[1];
	optind = 2;

	// Parse arguments
	while ((opt = getopt_long(argc, argv, "s:o:i:h", long_options, &option_index)) != -1) {
		switch (opt) {
			case 's':
				symbol_name = optarg;
				break;
			case 'o':
				output_file = optarg;
				break;
			case 'i':
				input_file = optarg;
				break;
			case 'h':
				symtool_show_help();
				return 0;
			default:
				symtool_show_help();
				return 1;
		}
	}

	if (optind < argc) {
		elf_file = argv[optind];
		optind++;
	}
	
	if (!elf_file) {
		fprintf(stderr, "Error: ELF file name required\n");
		symtool_show_help();
		return 1;
	}

	if (strcmp(command, "extract") == 0) {
		if (!symbol_name) {
			fprintf(stderr, "Error: Symbol name required for extraction\n");
			return 1;
		}
		
		if (!output_file) {
			fprintf(stderr, "Error: Output file name required for extraction\n");
			return 1;
		}
		
		ElfFile elf = load_elf(elf_file, 0);
		extract_symbol_binary(&elf, symbol_name, output_file);
		free_elf(&elf);
	} 
	else if (strcmp(command, "restore") == 0) {
		if (!symbol_name) {
			fprintf(stderr, "Error: Symbol name required for restoration\n");
			return 1;
		}
		
		if (!input_file) {
			fprintf(stderr, "Error: Input file name required for restoration\n");
			return 1;
		}
		
		ElfFile elf = load_elf(elf_file, 1);
		restore_symbol_binary(&elf, symbol_name, input_file);
		
		// Save
		int fd = open(elf_file, O_WRONLY);
		if (fd < 0) {
			FILE_ERROR("Failed to open '%s' for writing: %s", elf_file, strerror(errno));
		}
		
		if (write(fd, elf.data, elf.size) != (ssize_t)elf.size) {
			close(fd);
			FILE_ERROR("Failed to write to '%s': %s", elf_file, strerror(errno));
		}
		
		close(fd);
		printf("Successfully saved modified ELF to '%s'\n", elf_file);
		
		free_elf(&elf);
	} 
	else {
		fprintf(stderr, "Error: Unknown command '%s'\n", command);
		symtool_show_help();
		return 1;
	}
	
	return 0;
}

REGISTER_MODULE(symtool);
