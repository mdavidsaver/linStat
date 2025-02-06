#!../../bin/linux-x86_64/linStatDemo

# search PATH for dbLoadRecords()
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(PWD)/../../db")

epicsEnvSet("ENGINEER", "MD")

dbLoadDatabase "$(PWD)/../../dbd/linStatDemo.dbd"
linStatDemo_registerRecordDeviceDriver(pdbbase) 

#var linStatDebug 5

dbLoadRecords("linStatHost.db","IOC=LOCALHOST")
dbLoadRecords("linStatProc.db","IOC=LOCALHOST")
dbLoadRecords("linStatNIC.db","IOC=LOCALHOST,NIC=lo")
dbLoadRecords("linStatFS.db","P=LOCALHOST:ROOT,DIR=/")

dbLoadRecords("linStatNIC.db","IOC=LOCALHOST,NIC=wlp0s20f3")


iocInit()
