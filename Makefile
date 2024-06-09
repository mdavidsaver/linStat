# Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure

DIRS += statApp
statApp_DEPEND_DIRS = configure

ifeq (YES,$(LINSTAT_BUILD_EXAMPLE))
DIRS += exampleApp
exampleApp_DEPEND_DIRS = statApp

DIRS += $(wildcard iocBoot)
iocBoot_DEPEND_DIRS = exampleApp
endif # LINSTAT_BUILD_EXAMPLE

include $(TOP)/configure/RULES_TOP
