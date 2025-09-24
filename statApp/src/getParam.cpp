/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

/* getParam.cpp - String Device Support Routines for env vars and EPICS
 * parameters */

#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <cstdlib>
#include <iterator>
#include <cstring>
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

template<class Rec>
long showErr(Rec *prec, const char* func, std::exception& e) noexcept {
    recGblSetSevrMsg(prec, COMM_ALARM, INVALID_ALARM, "%s", e.what());
    errlogPrintf("%s " ERL_ERROR " %s : %s\n", prec->name, func, e.what());
    return -1;
}

static long getParam_init_record(dbCommon *pcom) {
    auto plink = dbGetDevLink(pcom);
    assert(plink && plink->type==INST_IO);
    std::string param_name = plink->value.instio.string;

    for (auto pParam = env_param_list; *pParam != nullptr; pParam++){
        if (param_name == (*pParam)->name) {
            pcom->dpvt = (void *) *pParam;
            return 0;
        }
    }
    pcom->dpvt = nullptr; // Assume it is not part of the env_param_list
    return 0;
}

static long getParam_read_stringin(stringinRecord *prec) {
    auto param = (ENV_PARAM *) prec->dpvt;
    std::string value;
    try {
        if (param == nullptr) {
            const char * param_name = prec->inp.value.instio.string;
            const char * val_ptr = std::getenv(param_name);

            if(!val_ptr){
                strcpy(prec->val,"");
                throw std::runtime_error("Env var not found");
            }

            strncpy(prec->val, val_ptr, MAX_STRING_SIZE-1);
            prec->val[MAX_STRING_SIZE-1] = '\0';
        }
        else {
            if (!envGetConfigParam((ENV_PARAM *)prec->dpvt, MAX_STRING_SIZE, prec->val)) {
                strcpy(prec->val, "Undefined");
                throw std::runtime_error("Variable is undefined");
            }
        }
    }
    catch(std::exception& e) {
        return showErr(prec, __func__, e);
    }
    prec->udf = 0;
    return 0;
}

/* dset for devGetParam */
const stringindset devGetParamSi = {
    {5, NULL, NULL, getParam_init_record, NULL},
    getParam_read_stringin
};

extern "C" {
epicsExportAddress(dset, devGetParamSi);
}
