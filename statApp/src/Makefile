TOP=../..

include $(TOP)/configure/CONFIG
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
LIB_SRCS_Linux += tblBase.cpp
# host
LIB_SRCS_Linux += tblStat.cpp
LIB_SRCS_Linux += tblHost.cpp
LIB_SRCS_Linux += tblMemInfo.cpp
LIB_SRCS_Linux += tblUptime.cpp
LIB_SRCS_Linux += tblNetstat.cpp
LIB_SRCS_Linux += tblIFStat.cpp
LIB_SRCS_Linux += tblThermo.cpp
# process
LIB_SRCS_Linux += tblPid.cpp
LIB_SRCS_Linux += tblProcStat.cpp
LIB_SRCS_Linux += tblProcStatus.cpp
LIB_SRCS_Linux += tblProcFD.cpp
LIB_SRCS_Linux += tblProcPDB.cpp
LIB_SRCS_Linux += tblProcMallinfo.cpp
# file system
LIB_SRCS_Linux += tblStatVFS.cpp

LIB_SRCS_DEFAULT += dummy.c

LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

