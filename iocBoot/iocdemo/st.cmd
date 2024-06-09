#!../../bin/linux-x86_64/linStatDemo

## Register all support components
dbLoadDatabase "../../dbd/linStatDemo.dbd"
linStatDemo_registerRecordDeviceDriver(pdbbase) 

#var linStatDebug 5

dbLoadRecords("../../db/linStatHost.db","IOC=LOCALHOST")
dbLoadRecords("../../db/linStatProc.db","IOC=LOCALHOST")
dbLoadRecords("../../db/linStatNIC.db","IOC=LOCALHOST,NIC=lo")
dbLoadRecords("../../db/linStatFS.db","P=LOCALHOST:ROOT,DIR=/")


iocInit()
