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
