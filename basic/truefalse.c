/**
 *	truefalse.c - Return 0 for true and 1 for false
 *
 * 	Created by RoofAlan
 *		2025/8/11
 *
 *	Copyright (C) 2025 ASO-Studio
 *	Based on MIT protocol open source
 */

#include "config.h"
#include "module.h"

int true_main(int argc, char *argv[]) {
	return 0;
}

int false_main(int argc, char *argv[]) {
	return 1;
}

REGISTER_MODULE(true);
REGISTER_MODULE(false);
REGISTER_MODULE2(true, ":");
