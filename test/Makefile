TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

USR_CPPFLAGS += -I$(TOP)/statApp/src

ifdef BASE_3_15

DBDDEPENDS_FILES += testioc.dbd$(DEP)
testioc_DBD = base.dbd linStat.dbd
#TESTFILES += $(COMMON_DIR)/testioc.dbd

TESTPROD_HOST += testLSIOC
testLSIOC_SRCS += testLSIOC.cpp
testLSIOC_SRCS += testioc_registerRecordDeviceDriver.cpp
TESTS += testLSIOC

endif # BASE_3_15

PROD_LIBS += linStat
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

