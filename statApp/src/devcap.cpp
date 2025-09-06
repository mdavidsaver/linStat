/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <string.h>

#include <linux/capability.h>

#include <aaiRecord.h>
#include <recGbl.h>
#include <alarm.h>
#include <menuFtype.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <epicsExport.h>

#ifndef HAS_ALARM_MESSAGE
#  define recGblSetSevrMsg(PREC, STAT, SEVR, ...) (void)recGblSetSevr(PREC, STAT, SEVR)
#endif

namespace {

struct cap_ent {
    unsigned bit;
    const char *name;
};

// a selection of capability names relevant to IOCs
// adjust associated NELM when extending
static const cap_ent cap_map[] = {
#define CASE(NAME) {CAP_ ## NAME, #NAME}
    CASE(NET_ADMIN),
    CASE(SYS_ADMIN),
    CASE(SYS_NICE),
    CASE(IPC_LOCK),
#undef CASE
};

long devLinCapArr(aaiRecord *prec) noexcept {
    if(prec->ftvl!=menuFtypeSTRING) {
        recGblSetSevrMsg(prec, SOFT_ALARM, INVALID_ALARM, "Bad FTVL");
        return -1;
    }
    epicsUInt64 mask = 0;
    if(dbGetLink(&prec->inp, DBR_UINT64, &mask, nullptr, nullptr)) {
        recGblSetSevrMsg(prec, LINK_ALARM, INVALID_ALARM, "INP");
        return -1;
    }

    auto val = reinterpret_cast<char*>(prec->bptr);

    size_t bit, out_idx=0;
    for(bit=0; mask && out_idx<prec->nelm; bit++, mask>>=1u) {
        if(!(mask&1))
            continue;

        for(size_t cap=0; cap<NELEMENTS(cap_map); cap++) {
            auto& ent = cap_map[cap];
            if(bit==ent.bit) {
                strncpy(val, ent.name, MAX_STRING_SIZE-1);
                val[MAX_STRING_SIZE-1] = '\0';
                out_idx++;
                val += MAX_STRING_SIZE;
                break;
            }
        }
    }
    prec->nord = out_idx;
    return 0;
}

const aaidset devLinStatsAaiCapArr = {
    {
        5,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    },
    devLinCapArr
};

} // namespace

extern "C" {
epicsExportAddress(dset, devLinStatsAaiCapArr);
}
