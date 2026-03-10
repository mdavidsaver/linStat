/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <sstream>
#include <string>
#include <stdint.h>

#include <stringinRecord.h>
#include <dbDefs.h>
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

struct Interval {
    uint64_t period; // seconds
    const char *label;
};
const Interval intervals[] = { // in descending order
    {60*60*24, "D"},
    {60*60, "H"},
    {60, "M"},
};

const stringindset devLinStatAiTDelta = {
    {
        5,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    },
    [](stringinRecord *prec) noexcept -> long {
        try {
            epicsInt32 uptime=0;
            auto sts = dbGetLink(&prec->inp, DBR_LONG, &uptime, nullptr, nullptr);
            if(!sts) {
                std::ostringstream msg;
                for(size_t i=0; i<NELEMENTS(intervals); i++) {
                    const auto& ivl = intervals[i];
                    if(auto cnt = uptime / ivl.period) {
                        msg<<cnt<<" "<<ivl.label<<" ";
                        uptime %= ivl.period;
                    }
                }
                msg<<(unsigned long)uptime<<" S";
                auto s(msg.str());
                s.copy(prec->val, NELEMENTS(prec->val)-1);
                prec->val[NELEMENTS(prec->val)-1] = '\0';
            }
            return sts;
        }catch(std::exception& e){
            recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "%s", e.what());
            return -1;
        }
    }
};

} // namespace

extern "C" {
epicsExportAddress(dset, devLinStatAiTDelta);
}
