#ifndef ELFOP_H
#define ELFOP_H

typedef struct {
	int fd;		// File descriptor
	const char *fname;	// File name
	unsigned char ident[16];// ELF ident
} ElfFileInfo;


typedef struct {
	int b32;	// Is 32-bits
	enum {
		B_ENDIAN, // Big endian
		L_ENDIAN, // Little endian
		I_ENDIAN, // Invalid endian
	} endian;	// Endian
	int osabi;	// OS ABI
	int version;	// Version
	const char *abiName;	// Name of the OS ABI
	const char *machine;	// Machine
	const char *type;	// ELF Type
	int pie;	// Is PIE
	int dynamic;	// Is dynamic
	char *inter;	// Interrupter
	int stripped;	// Is stripped
	int debug_info;	// Have debug info
} ElfInfo;

/* Check if a file is an ELF */
int isElf(const char *file);

/* Open ELF*/
int openElf(ElfFileInfo *, const char *name);

/* Close ELF */
void closeElf(ElfFileInfo *);

/* Parse Elf */
int parseElf(ElfFileInfo *, ElfInfo *);

#endif
