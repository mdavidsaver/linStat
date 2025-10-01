/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

/* getParam.c - String Device Support Routines for env vars and EPICS
 * parameters */

#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <stdlib.h>
#include <string.h>
#include <dbCommon.h>
#include <alarm.h>
#include <errlog.h>
#include <recGbl.h>
#include <stringinRecord.h>
#include <devSup.h>
#include <epicsExport.h>
#include <envDefs.h>


#ifndef HAS_ALARM_MESSAGE
#  define recGblSetSevrMsg(PREC, STAT, SEVR, ...) (void)recGblSetSevr(PREC, STAT, SEVR)
#endif

static long getParam_init_record(dbCommon *pcom) {
    DBLINK* plink = dbGetDevLink(pcom);
    assert(plink && plink->type==INST_IO);
    const char* param_name = plink->value.instio.string;

    // first attempt look-up of provided name as "parameter"
    for (const ENV_PARAM** pParam = env_param_list; *pParam != NULL; pParam++){
        if (strcmp(param_name, (*pParam)->name)==0) {
            pcom->dpvt = (void *) *pParam;
            return 0;
        }
    }
    return 0;
}

static long getParam_read_stringin(stringinRecord *prec) {
    ENV_PARAM* param = (ENV_PARAM *) prec->dpvt;
    const char* val;
    if(param) { // read parameter value, or default
        val = envGetConfigParamPtr(param);
        if(!val)
            recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "No Default");

    } else { // fetch from environment
        val = getenv(prec->inp.value.instio.string);
        if(!val)
            recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "Not set");
    }
    if(val) {
        size_t vlen = strlen(val);
        if(vlen >= sizeof(prec->val)) { // sizeof() == strlen()+1
            vlen = sizeof(prec->val)-4; // space to append "..."
            strcpy(prec->val + vlen, "...");
            recGblSetSevrMsg(prec, COMM_ALARM, MINOR_ALARM, "Truncate");

        } else {
            vlen += 1; // copy nil from source
        }
        memcpy(prec->val, val, vlen);
        prec->udf = 0;
    }
    return 0;
}

/* dset for devGetParam */
static
const stringindset devGetParamSi = {
    {5, NULL, NULL, getParam_init_record, NULL},
    getParam_read_stringin
};

epicsExportAddress(dset, devGetParamSi);
