/*
 * module.c - 功能注册系统实现
 */
#include "module.h"
#include <string.h>

/* Global module registry header pointer */
static ModApi *module_registry = NULL;

/* Platform-specific segment boundary declarations */
#if defined(__linux__) || defined(__unix__)
	// Linux/Unix: Standard GCC Symbol
	extern ModApi __start_module_section[];
	extern ModApi __stop_module_section[];
#elif defined(__APPLE__)
	// macOS: Special symbol
	extern ModApi __start_module_section[] __asm("section$start$__DATA$__module_section__");
	extern ModApi __stop_module_section[] __asm("section$end$__DATA$__module_section__");
#elif defined(_WIN32) && defined(_MSC_VER)
	// Windows MSVC: pragma section + global symbol
	__declspec(allocate("__module_section__")) ModApi *__start_module_section;
	__declspec(allocate("__module_section__")) ModApi *__stop_module_section;
#else
	ModApi *__start_module_section = NULL;
	ModApi *__stop_module_section = NULL;
#endif

// 插入排序将功能按优先级排序
static void insert_sorted(ModApi *new_module) {
	if (!module_registry || new_module->priority < module_registry->priority) {
		new_module->next = module_registry;
		module_registry = new_module;
		return;
	}
	
	ModApi *current = module_registry;
	while (current->next && current->next->priority <= new_module->priority) {
		current = current->next;
	}
	
	new_module->next = current->next;
	current->next = new_module;
}

// 初始化功能注册表
void module_registry_init(void) {
	// 如果已经初始化则返回
	static int initialized = 0;
	if (initialized) return;
	initialized = 1;
	
	//printf("Initializing module registry...\n");
	
#if !(defined(__linux__) || defined(__APPLE__) || (defined(_WIN32) && defined(_MSC_VER)))
	// 不支持自动注册的平台需要手动注册
	printf("WARNING: Manual registration required on this platform\n");
	return;
#endif

	// 计算功能数量
	size_t count = (size_t)((char*)__stop_module_section - (char*)__start_module_section) / sizeof(ModApi);
	//printf("Found %zu modules in registry\n", count);
	
	// 遍历所有功能并插入排序链表
	for (ModApi *f = __start_module_section; f < __stop_module_section; f++) {
		// 创建副本（原始数据在只读段）
		ModApi *new_module = malloc(sizeof(ModApi));
		if (!new_module) {
			fprintf(stderr, "Memory allocation failed\n");
			exit(1);
		}
		*new_module = *f;
		insert_sorted(new_module);
	}
}

// 打印所有注册功能
void list_all_modules(void) {
	module_registry_init();

	for (ModApi *f = module_registry; f; f = f->next) {
		printf("%s ", f->name);
	}
	printf("\n");
}

int find_module(const char *name) {
	module_registry_init();

	for(ModApi *f = module_registry; f; f = f->next) {
		if(strcmp(f->name, name) == 0)
			return 0;
	}
	return 1;
}

int run_module(const char* name, int argc, char **argv) {
	module_registry_init();

	int suc = 1; // Default = 1 (Failed)

	for (ModApi *f = module_registry; f; f = f->next) {
		if(strcmp(name, f->name) == 0){
			suc = 0;
			if(f->main) {
				suc = f->main(argc, argv);
			}
		}
	}
	return suc;
}
