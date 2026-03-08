TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

DB += linStatHost.db
DB += linStatProc.db
DB += linStatFS.db
DB += linStatNIC.db
# indirect dep.  not installed
DBDDEPENDS_FILES += netStat.db$(DEP)

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

linStatHost.db$(DEP): $(COMMON_DIR)/netStat.db
linStatProc.db$(DEP): $(COMMON_DIR)/netStat.db
