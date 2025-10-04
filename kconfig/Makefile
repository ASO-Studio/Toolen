# ===========================================================================
# Kernel configuration targets
# These targets are used from top-level makefile
# ===========================================================================

KCONFIG_TOP = Kconfig
KCONFIG_PROJECT = Toolen
obj = ./kconfig
PHONY += clean menuconfig

GENER = bash scripts/finalGen.sh .config generated/sources.mk && bash scripts/genDefs.sh .config generated/defs.mk

.PHONY: menucofnig allyesconfig
menuconfig: $(obj)/mconf $(KCONFIG_TOP)
	@bash scripts/optCmd.sh
	@$< $(KCONFIG_TOP)
	@$(GENER)

allyesconfig:
	@bash scripts/optCmd.sh
	@bash scripts/allYes.sh

SHIPPED = kconfig/zconf.tab.c kconfig/lex.zconf.c kconfig/zconf.hash.c

%.c: %.c_shipped
	@ln -s $(notdir $<) $@

kconfig/mconf: $(SHIPPED)
	$(HOSTCC) -o $@ kconfig/mconf.c kconfig/zconf.tab.c \
		kconfig/lxdialog/*.c -lcurses -DCURSES_LOC="<ncurses.h>" \
		-DKBUILD_NO_NLS=1 -DPROJECT_NAME=\"$(KCONFIG_PROJECT)\"

kconfig/conf: $(SHIPPED)
	$(HOSTCC) -o $@ kconfig/conf.c kconfig/zconf.tab.c -DKBUILD_NO_NLS=1 \
		-DPROJECT_NAME=\"$(KCONFIG_PROJECT)\"

kconfig_clean:
	@rm -f $(wildcard kconfig/*zconf*.c) kconfig/conf kconfig/mconf
