/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifdef __linux__
#  error Linux is no dummy!
#endif

#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <stdlib.h>

#include <epicsExport.h>
#include <devSup.h>
#include <drvSup.h>

static
int linStatDebug = -1;

static
void linStatRegistrar() {}

const drvet drvLinStat = {
    2,
    NULL,
    NULL,
};

epicsExportAddress(drvet, drvLinStat);
epicsExportRegistrar(linStatRegistrar);
epicsExportAddress(int, linStatDebug);

#define DUMMY_DSET(NAME) \
    static const dset NAME = {4, NULL,NULL,NULL,NULL}; \
    epicsExportAddress(dset, NAME)
DUMMY_DSET(devLinStatBoScan);
DUMMY_DSET(devLinStatAiTblTime);
DUMMY_DSET(devLinStatI64iValue);
DUMMY_DSET(devLinStatSiValue);
