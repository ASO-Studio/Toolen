ifeq ($(VERBOSE), 1)
	V=
	Q=
else 
	V=@printf "  Compiling ==> $< ...         \n"; printf " Compiling all objects...\r";
	Q=@
endif
CC = gcc
C_FLAGS = -c -MMD -Iinclude/
LD_FLAGS = 

SOURCES = $(shell find -name "*.c")
OBJS = $(SOURCES:%.c=%.o)
DEPS = $(OBJS:%.o=%.d)

OUTPUT = toolen

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(Q)printf "  Linking ==> $(OUTPUT)...        \n"; printf " All objects was compiled\n"
	$(Q)$(CC) $(LD_FLAGS) -o $(OUTPUT) $(OBJS)

%.o: %.c
	$(V)$(CC) $(C_FLAGS) $< -o $@

clean:
	$(Q)printf "[Clean] $(shell basename $(shell pwd))\n"
	$(Q)rm -rf $(OBJS) $(OUTPUT) $(DEPS)

-include $(DEPS)
