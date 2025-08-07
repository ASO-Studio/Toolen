/*
 * module.h - 功能注册系统头文件
 * 支持平台：Linux/macOS/Windows (GCC/Clang/MSVC)
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// 功能描述结构体
typedef struct ModApi {
	const char *name;		// 功能名称
	int (*main)(int,char**);	// 主函数
	int priority;
	struct ModApi *next;	// 链表指针

} ModApi;

// 平台特定的段属性声明
#if defined(__GNUC__) || defined(__clang__)
	// GCC/Clang 使用section属性
	#define FEATURE_SECTION __attribute__((section("module_section"), used))
#elif defined(_MSC_VER)
	// MSVC 使用pragma section + allocate
	#pragma section("module_section", read)
	#define FEATURE_SECTION __declspec(allocate("module_section"))
#else
	// 其他编译器不支持段属性
	#define FEATURE_SECTION
	#warning "Unsupported compiler, using basic registration"
#endif

/**
 * 模块注册宏
 * @param module_name 功能名称(需要实现 module_name_init 和 module_name_run 函数)
 */
#define REGISTER_MODULE(module_name) \
	static void module_name##_init(void); \
	static void module_name##_run(void); \
	static ModApi _module_##module_name FEATURE_SECTION = { \
		.name = #module_name, \
		.main = module_name##_main, \
		.priority = 0,	\
		.next = NULL \
	}

// 初始化功能注册表
void module_registry_init(void);

// 打印所有注册功能
void list_all_modules(void);

// 寻找指定模块
int find_module(const char*);

// 运行指定模块
int run_module(const char*, int, char**);

#ifdef __cplusplus
}
#endif
