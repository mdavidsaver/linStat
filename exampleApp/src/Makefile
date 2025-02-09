TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

PROD_IOC_Linux = linStatDemo
DBD += linStatDemo.dbd

linStatDemo_DBD += base.dbd

linStatDemo_SRCS += linStatDemo_registerRecordDeviceDriver.cpp
linStatDemo_SRCS += exampleMain.cpp

ifdef LINSTAT_0_0_0
linStatDemo_DBD += linStat.dbd
linStatDemo_LIBS += linStat
endif

# Link QSRV
ifdef PVXS_MAJOR_VERSION # prefer v2
linStatDemo_DBD += pvxsIoc.dbd
linStatDemo_LIBS += pvxsIoc pvxs
else
ifdef EPICS_QSRV_MAJOR_VERSION # fallback to v1
linStatDemo_LIBS += qsrv
linStatDemo_LIBS += $(EPICS_BASE_PVA_CORE_LIBS)
linStatDemo_DBD += PVAServerRegister.dbd
linStatDemo_DBD += qsrv.dbd
endif
endif

linStatDemo_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

