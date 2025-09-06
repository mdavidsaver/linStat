TOP=../..

include $(TOP)/configure/CONFIG
ifdef T_A
include $(TOP)/configure/O.$(T_A)/TOOLCHAIN
endif
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += linStat

DBD += linStat.dbd

# specify all source files to be compiled and added to the library
LIB_SRCS_Linux += iochooks.cpp
LIB_SRCS_Linux += devstat.cpp
LIB_SRCS_Linux += nlreact.cpp
LIB_SRCS_Linux += lsutils.cpp
LIB_SRCS_Linux += tblBase.cpp
# host
LIB_SRCS_Linux += tblStat.cpp
LIB_SRCS_Linux += tblHost.cpp
LIB_SRCS_Linux += tblMemInfo.cpp
LIB_SRCS_Linux += tblUptime.cpp
LIB_SRCS_Linux += tblNetstat.cpp
LIB_SRCS_Linux += tblIFStat.cpp
LIB_SRCS_Linux += tblHwmon.cpp
LIB_SRCS_Linux += tblEthtool.cpp
# process
LIB_SRCS_Linux += tblPid.cpp
LIB_SRCS_Linux += tblProcStat.cpp
LIB_SRCS_Linux += tblProcStatus.cpp
LIB_SRCS_Linux += tblProcFD.cpp
LIB_SRCS_Linux += tblProcPDB.cpp
LIB_SRCS_Linux += tblProcMallinfo.cpp
# file system
LIB_SRCS_Linux += tblStatVFS.cpp

LIB_SRCS_Linux += devcap.cpp

LIB_SRCS_DEFAULT += dummy.c

LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

ifeq (YES,$(LINSTAT_NEED_STDCXXFS))
# see configure/toolchain.cpp
LIB_SYS_LIBS += stdc++fs
endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

