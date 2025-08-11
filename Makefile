V=@printf "  Compiling ==> $(shell basename $<) ...         \n"; printf " Compiling all objects...\r";
Q=@

HOSTCC = gcc
CC = gcc
C_FLAGS = -c -MMD -Iinclude/
LD_FLAGS =

# Include source file
ifneq ($(wildcard generated/sources.mk),)
 include generated/sources.mk
else
 SOURCES :=
endif

# Include config file
ifneq ($(wildcard .config),)
 include .config
endif

# Parse build options
# Static link
#  TIPS: It will cause some warnings such as 'Using xxx in statically linked application'
#  TODO: Fix warnings
ifeq ($(CONFIG_STATIC_LINK),y)
 LD_FLAGS += -static
endif

# Strip
ifeq ($(CONFIG_STRIP),y)
 LD_FLAGS += -s
endif

OBJS = $(SOURCES:%.c=%.o)
DEPS = $(OBJS:%.o=%.d)

OUTPUT = toolen

all: check $(OUTPUT)

check:
	$(Q)if [ ! -f "generated/sources.mk" ]; then \
		echo "No sources.mk file found, please run 'make menuconfig' or 'make allyesconfig' first!"; \
		exit 1; \
	fi

$(OUTPUT): $(OBJS)
	$(Q)printf "  Linking ==> $(OUTPUT)...        \n"; printf " All objects was compiled\n"
	$(Q)$(CC) $(LD_FLAGS) -o $(OUTPUT) $(OBJS)

%.o: %.c
	$(V)$(CC) $(C_FLAGS) $< -o $@

clean:
	$(Q)printf "[Clean] $(shell basename $(shell pwd))\n"
	$(Q)rm -rf $(OBJS) $(OUTPUT) $(DEPS)

cleanall: clean kconfig_clean
	$(Q)rm -rf .config generated .config.old

# menuconfig
-include kconfig/Makefile

# Dependence
-include $(DEPS)
