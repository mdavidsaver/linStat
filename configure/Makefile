TOP=..

include $(TOP)/configure/CONFIG

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

CFG += CONFIG_LINSTAT_VERSION

include $(TOP)/configure/RULES

ifdef T_A
install: TOOLCHAIN

TOOLCHAIN: toolchain.cpp
	$(CXX) -E $< | grep '^LINSTAT_' > $@
endif
