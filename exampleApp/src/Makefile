TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

PROD_IOC_Linux = linStatDemo
DBD += linStatDemo.dbd

linStatDemo_DBD += base.dbd
linStatDemo_DBD += linStat.dbd

linStatDemo_SRCS += linStatDemo_registerRecordDeviceDriver.cpp
linStatDemo_SRCS += exampleMain.cpp

linStatDemo_LIBS += linStat
linStatDemo_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

