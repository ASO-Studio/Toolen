V=@printf "\033[J  Compiling ==> $(shell basename $<) ...\n"; printf " Compiling all objects...\r";
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

ifneq ($(wildcard generated/defs.mk),)
 include generated/defs.mk
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

# Sometimes it will be like 'CONFIG... is not set', and sometimes it will be 'CONFIG_...=""'
# Set to new compiler if CONFIG_C_CPLR is not empty
ifneq ($(CONFIG_C_CPLR),"")
 CC = $(CONFIG_C_CPLR)
 ifeq ($(CONFIG_C_CPLR),)
  CC = gcc
 endif
endif

# Host C compiler is same as above
ifneq ($(CONFIG_HOST_C_CPLR),"")
 HOSTCC = $(CONFIG_HOST_C_CPLR)
 ifeq ($(CONFIG_HOST_C_CPLR),)
  HOSTCC = gcc
 endif
endif

# Strip
ifeq ($(CONFIG_STRIP),y)
 LD_FLAGS += -s
endif

# Compile license
ifeq ($(CONFIG_COMPILE_LICENSE),y)
 C_FLAGS += -DCOMPILE_LICENSE
endif

# Debug support
ifeq ($(CONFIG_DEBUG_SPT),y)
 C_FLAGS += -g
endif

# Memory leak check
ifeq ($(CONFIG_MEMLEAK_CHECK),y)
 C_FLAGS += -fsanitize=address
 LD_FLAGS += -lasan
endif

C_FLAGS += -DCCVER='"$(shell $(CC) --version | head -n1)"' -DAPPEND='"'$(CONFIG_VERSION_APPEND)'"'

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
