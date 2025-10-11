/*
 * elf.c - ELF Operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <elf.h>

#include "lib.h"

int isElf(const char *file) {
	int fd = xopen2(file, O_RDONLY);

	// Read magic number
	char ident[16];
	read(fd, ident, sizeof(ident));

	xclose(fd);
	return memcmp(ident, ELFMAG, SELFMAG) == 0;
}

int openElf(ElfFileInfo *f, const char *name) {
	f->fd = xopen2(name, O_RDONLY);

	// Check magic number ("\x7f" "ELF")
	Elf64_Ehdr ehdr;
	read(f->fd, &ehdr, sizeof(ehdr));
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
		return 1;
	}

	// Copy ident
	memcpy(f->ident, ehdr.e_ident, sizeof(f->ident));

	// Reset file cursor
	lseek(f->fd, 0, SEEK_SET);

	f->fname = name;

	return 0;
}

void closeElf(ElfFileInfo *f) {
	if (f->fd > 0)
		xclose(f->fd);
}

static const char *getMachineName(int em) {
	switch(em) {
		case EM_SPARC: return "Sparc";
		case EM_386: return "Intel 80386";
		case EM_IAMCU: return "Intel MCU";
		case EM_860: return "Intel 80860";
		case EM_MIPS: return "Mips";
		case EM_MIPS_RS3_LE: return "Mips R3000 Little endian";
		case EM_960: return "Intel 80960";
		case EM_PPC: return "PowerPC";
		case EM_PPC64: return "PowerPC 64";
		case EM_ARM: return "ARM";
		case EM_SPARCV9: return "Sparc V9 64-bit";
		case EM_IA_64: return "Intel Merced";
		case EM_X86_64: return "x86-64";
		case EM_8051: return "Intel 8051";
		case EM_QDSP6: return "Qualcomm DSP6";
		case EM_AARCH64: return "AArch64";
		case EM_STM8: return "STM 8";
		case EM_CUDA: return "Nvidia CUDA";
		case EM_AMDGPU: return "AMD GPU";
		case EM_RISCV: return "RISC-V";
		case EM_BPF: return "Linux BPF";
		case EM_LOONGARCH: return "Loongarch";
		default: break;
	}
	return "Unknown";
}

static const char *getTypeName(int et) {
	switch(et) {
		case ET_REL: return "relocatable";
		case ET_EXEC: return "executable";
		case ET_DYN: return "shared object";
		case ET_CORE: return "core file";
		default: break;
	}
	return "Unknown";
}

static const char *getOSABIName(int oabi) {
	switch(oabi) {
		case ELFOSABI_SYSV: return "SYSV";
		case ELFOSABI_HPUX: return "HPUX";
		case ELFOSABI_NETBSD: return "NetBSD";
		case ELFOSABI_GNU: return "GNU";
		case ELFOSABI_SOLARIS: return "Solaris";
		case ELFOSABI_AIX: return "AIX";
		case ELFOSABI_FREEBSD: return "FreeBSD";
		case ELFOSABI_TRU64: return "TRU64";
		case ELFOSABI_MODESTO: return "Modesto";
		case ELFOSABI_OPENBSD: return "OpenBSD";
		case ELFOSABI_ARM_AEABI: return "ARM EABI";
		case ELFOSABI_ARM: return "ARM";
		case ELFOSABI_STANDALONE: return "Standalone";
		default: break;
	}
	return "Unknown";
}

static char *getInterrupter(ElfFileInfo *f, ElfInfo *ei, Elf64_Ehdr ehdr, Elf32_Ehdr ehdr32) {
	int phnum = 0;
	if (!(ei->dynamic))	// Static executables dont have interruptor
		return NULL;

	if (ei->b32) {
		lseek(f->fd, ehdr32.e_phoff, SEEK_SET);
		phnum = ehdr32.e_phnum;
	} else {
		lseek(f->fd, ehdr.e_phoff, SEEK_SET);
		phnum = ehdr.e_phnum;
	}

	Elf64_Phdr phdr;
	Elf32_Phdr phdr32;
	int foundInter = 0;
	char *interp = NULL;

	for (int i = 0; i < phnum; i++) {
		if (ei->b32) {
			read(f->fd, &phdr32, sizeof(phdr32));
			if (phdr32.p_type == PT_INTERP)
				foundInter = 1;
		} else {
			read(f->fd, &phdr, sizeof(phdr));
			if (phdr.p_type == PT_INTERP)
				foundInter = 1;
		}

		// Not found, exit
		if (!foundInter)
			continue;

		// Read interrupter
		size_t size = 0;
		if (ei->b32) {
			lseek(f->fd, phdr32.p_offset, SEEK_SET);
			size = phdr32.p_filesz;
		} else {
			lseek(f->fd, phdr.p_offset, SEEK_SET);
			size = phdr.p_filesz;
		}

		if (!size) break;
		interp = xmalloc(size);
		read(f->fd, interp, size);
		break;
	}

	return interp;
}

void is_stripped(ElfFileInfo *f, ElfInfo *ei, Elf64_Ehdr ehdr, Elf32_Ehdr ehdr32) {
	int fd = f->fd;

	lseek(fd, ei->b32 ? ehdr32.e_shoff : ehdr.e_shoff, SEEK_SET);
	ei->stripped = 1;
	ei->debug_info = 0;

	Elf64_Shdr shdr;
	Elf32_Shdr shdr32;
	for (int i = 0; i < ehdr.e_shnum; i++) {
		if (ei->b32) {
			lseek(fd, ehdr32.e_shoff + i * ehdr32.e_shentsize, SEEK_SET);
			read(fd, &shdr32, sizeof(shdr32));
		} else {
			lseek(fd, ehdr.e_shoff + i * ehdr.e_shentsize, SEEK_SET);
			read(fd, &shdr, sizeof(shdr));
		}


		if (shdr.sh_type == SHT_SYMTAB ||   // Symbol table
			shdr.sh_type == SHT_DYNSYM ||   // Dynamic symbol table
			shdr.sh_type == SHT_STRTAB ||   // String table
			shdr.sh_type == SHT_PROGBITS || // Program data
			shdr32.sh_type == SHT_SYMTAB ||   // Symbol table(32)
			shdr32.sh_type == SHT_DYNSYM ||   // Dynamic symbol table(32)
			shdr32.sh_type == SHT_STRTAB ||   // String table(32)
			shdr32.sh_type == SHT_PROGBITS) { // Program data(32)

			// Read sections
			char name[64] = {0};
			Elf64_Shdr strtab;
			Elf32_Shdr strtab32;
			if (ei->b32) {
				lseek(fd, ehdr32.e_shoff + ehdr32.e_shstrndx * ehdr32.e_shentsize, SEEK_SET);
				read(fd, &strtab32, sizeof(strtab32));
				lseek(fd, strtab32.sh_offset + shdr32.sh_name, SEEK_SET);
			} else {
				lseek(fd, ehdr.e_shoff + ehdr.e_shstrndx * ehdr.e_shentsize, SEEK_SET);
				read(fd, &strtab, sizeof(strtab));
				lseek(fd, strtab.sh_offset + shdr.sh_name, SEEK_SET);
			}

			read(fd, name, sizeof(name) - 1);

			if (strcmp(name, ".debug_info") == 0 ||
				strcmp(name, ".debug_line") == 0 ||
				strcmp(name, ".debug_abbrev") == 0) {
				ei->debug_info = 1;
			}

			// Check special section name
			if (strcmp(name, ".symtab") == 0 || 
				strcmp(name, ".strtab") == 0 ||
				ei->debug_info) {

				ei->stripped = 0;
				break;
			}
		}
	}
}

int parseElf(ElfFileInfo *f, ElfInfo *ei) {
	if (f->fd < 0)
		return 1;

	Elf64_Ehdr ehdr;
	Elf32_Ehdr ehdr32;
	read(f->fd, &ehdr, sizeof(ehdr));

	// Get the bits of the ELF
	switch(f->ident[EI_CLASS]) {
		case ELFCLASS32:
			ei->b32 = 1;
			lseek(f->fd, 0, SEEK_SET);
			read(f->fd, &ehdr32, sizeof(ehdr32));
			break;
		case ELFCLASS64:
			ei->b32 = 0;
			break;
		default:
			fprintf(stderr, "Cannot get the digit of the ELF\n");
			return 1;
	}

	// Get the endian of the ELF
	switch(f->ident[EI_DATA]) {
		case ELFDATA2LSB:
			ei->endian = L_ENDIAN;
			break;
		case ELFDATA2MSB:
			ei->endian = B_ENDIAN;
			break;
		default:
			ei->endian = I_ENDIAN;
			break;
	}

	// Get the OS ABI of the ELF
	ei->osabi = f->ident[EI_OSABI];

	// Get the OS ABI Name of the ELF
	ei->abiName = getOSABIName(ei->osabi);

	// Get the version of the ELF
	ei->version = ei->b32 ? ehdr32.e_version : ehdr.e_version;

	// Get the machine of the ELF
	ei->machine = getMachineName(ei->b32 ? ehdr32.e_machine : ehdr.e_machine);

	// Get the type of the ELF
	ei->type = getTypeName(ei->b32 ? ehdr32.e_type : ehdr.e_type);

	// Is dynamic
	ei->dynamic = ei->b32 ? ehdr32.e_type != ET_EXEC : ehdr.e_type != ET_EXEC;

	// Is PIE
	ei->pie = ei->dynamic;	// TODO: More comprehensive testing

	if (ei->dynamic) {
		// Get the interrupter
		ei->inter = getInterrupter(f, ei, ehdr, ehdr32);
	}

	// If it has interrupter, change the type to 'executable'
	if (ei->inter)
		ei->type = "executable";

	// Is stripped
	is_stripped(f, ei, ehdr, ehdr32);

	return 0;
}
