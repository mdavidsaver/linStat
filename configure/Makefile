TOP=..

include $(TOP)/configure/CONFIG

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

CFG += CONFIG_LINSTAT_VERSION

TARGETS += TOOLCHAIN

include $(TOP)/configure/RULES

TOOLCHAIN: toolchain.cpp
	$(CCC) -E $< | grep '^LINSTAT_' > $@
