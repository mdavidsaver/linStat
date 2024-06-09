# Linux Statistics Driver

An EPICS Driver to serve up Linux system and/or process
specific information from an IOC.

## Including

Starting from a template produced by:

```sh
makeBaseApp.pl -t ioc my
makeBaseApp.pl -t ioc -i -p myApp myioc
```

Edit `myApp/src/Makefile`.

```make
# ... myApp/src/Makefile
PROD_IOC += myapp
DBD += myapp.dbd
...
myapp_SRCS += myapp_registerRecordDeviceDriver.cpp
myapp_DBD += base.dbd

myapp_DBD += linStat.dbd # <----- Add
myapp_LIBS += linStat    # <----- Add

myapp_LIBS +=  $(EPICS_BASE_IOC_LIBS)
```

Edit `iocBoot/iocmyioc/st.cmd`.

```sh
# iocBoot/iocmyioc/st.cmd
#!../../linux-x86_64/myapp
# ...
dbLoadDatabase "../../dbd/myapp.dbd"
myapp_registerRecordDeviceDriver(pdbbase)

# Replace "LOCALHOST" with a unique record name prefix
epicsEnvSet("IOC", "LOCALHOST")

# Optional information
#epicsEnvSet("ENGINEER", "Respectible Name")
#epicsEnvSet("LOCATION", "Where am I?")

# (un)comment some or all of the following dbLoadRecords before iocInit.

# System-wide information
dbLoadRecords("../../db/linStatHost.db","IOC=$(IOC)")

# IOC process specific information
dbLoadRecords("../../db/linStatProc.db","IOC=$(IOC)")

# Network interface information
dbLoadRecords("../../db/linStatNIC.db","IOC=$(IOC),NIC=lo")
# may repeat with different NIC= network interface name(s)
#dbLoadRecords("../../db/linStatNIC.db","IOC=$(IOC),NIC=eth")

# File system mount point information
dbLoadRecords("../../db/linStatFS.db","P=$(IOC):ROOT,DIR=/")
# may repeat with different file system mount points
# change both P= and DIR=
#dbLoadRecords("../../db/linStatFS.db","P=$(IOC):DATA,DIR=/data")

iocInit
```

## Comparison with iocStats

This module provides a super-set for the information provided by
[iocStats](https://github.com/epics-modules/iocStats)
where possible with the same record names.
However unlike iocStats, this module is limited to Linux.

| linStat | iocStats | Description |
|---------|----------|-------------|
|    X    |     X    | CPU Usage   |
|    X    |     X    | Mem Usage   |
|    X    |     X    | FD Usage    |
|    X    |          | Swap Usage  |
|    X    |          | IP Stats.   |
|    X    |          | NIC Stats.  |
|    X    |          | FS Usage    |
|         |     X    | DB Scan stats. (TODO)  |

Additional information provided by linStat includes:

- `$(IOC):SYS_TOT_LOAD` CPU usage including overheads (IRQ handling, I/O waits, ...)
- IP stack statistics, host and process
- Network Interface Card (NIC) statistics
- File System usage

## TODO

- OPIs
    - Decide to convert/extend iocStats `.bob` or re-do?
- PDB
    - scan tasks
        - [ ] execution time (diff time between `PHAS=-32768` and `PHAS=32767`)
        - [ ] scan period (diff time between successive scans)
    - callback pool info
    - scan once info
- System
- Process
    - [ ] `epicsExitLater()`
    - [ ] Memory locked by `mlockall()`
    - [ ] Context switch counts
    - [ ] Page fault counts
    - [ ] Capabilities mask?
- FS
- NIC
