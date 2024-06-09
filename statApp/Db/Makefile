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

$(COMMON_DIR)/linStatHost.db: $(COMMON_DIR)/netStat.db
$(COMMON_DIR)/linStatProc.db: $(COMMON_DIR)/netStat.db

linStatHost.db$(DEP): netStat.db$(DEP)
linStatProc.db$(DEP): netStat.db$(DEP)
