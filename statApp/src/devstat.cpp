/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <stdexcept>
#include <regex>

#include <dbAccess.h>
#include <recGbl.h>
#include <alarm.h>
#include <errlog.h>
#include <epicsStdio.h>
#include <devSup.h>
#include <dbCommon.h>
#include <boRecord.h>
#include <biRecord.h>
#include <mbbiRecord.h>
#include <mbbiDirectRecord.h>
#include <longinRecord.h>
#include <int64inRecord.h>
#include <aiRecord.h>
#include <stringinRecord.h>
#include <epicsExport.h>

#include "linStat.h"

namespace {
using namespace linStat;

struct Pvt {
    std::shared_ptr<StatTable> tbl;
    std::string param;
    JobHandle scanHandle;
};

long devLinInitRecord(dbCommon *pcom) noexcept {
    auto plink = dbGetDevLink(pcom);
    assert(plink && plink->type==INST_IO);
    const auto lstr = plink->value.instio.string;
    try {
        std::unique_ptr<Pvt> pvt{new Pvt};

        // <factory>|<inst>|<param>
        static const std::regex expr(R"(([a-zA-Z]+)\|((?:[^|\\]|\\.)*)\|([a-zA-Z0-9:_]+)(?:\s.*)?)");
        std::cmatch M;
        if(!std::regex_match(lstr, M, expr))
            throw std::runtime_error("Invalid format");
        assert(M.size()==4);

        auto fact(M[1].str());
        auto inst(M[2].str());
        auto param(M[3].str());

        pvt->tbl = StatTable::lookup(fact, inst, linStatReactor);
        pvt->param = param;
        pcom->dpvt = pvt.release();

        return 0;
    }catch(std::exception& e){
        epicsPrintf("%s " ERL_ERROR " %s, \"%s\" : %s\n", pcom->name, __func__, lstr, e.what());
        return -1;
    }
}

Pvt* getPvt(void *praw) noexcept {
    auto pcom = static_cast<dbCommon*>(praw);
    auto pvt = static_cast<Pvt*>(pcom->dpvt);
    if(!pvt) {
        recGblSetSevrMsg(pcom, COMM_ALARM, INVALID_ALARM, "Init Fail");
    }
    return pvt;
}

long showErr(void *praw, const char* func, std::exception& e) noexcept {
    auto pcom = static_cast<dbCommon*>(praw);
    recGblSetSevrMsg(pcom, COMM_ALARM, INVALID_ALARM, "%s", e.what());
    errlogPrintf("%s " ERL_ERROR " %s : %s\n", pcom->name, func, e.what());
    return -1;
}

#define TRY auto pvt(getPvt(prec)); if(!pvt) return -1; try

#define CATCH() catch(std::exception& e) { return showErr(prec, __func__, e); }

long devLinTblIoIntr(int detach, struct dbCommon *pcom, IOSCANPVT* pscan) noexcept
{
    (void)detach;
    auto pvt = static_cast<Pvt*>(pcom->dpvt);
    if(pvt && pvt->tbl) {
        *pscan = pvt->tbl->scan;
        return 0;
    }
    errlogPrintf("%s " ERL_ERROR " no I/O Intr\n", pcom->name);
    return -1;
}

const bodset devLinStatBoScan = {
    {
        5,
        nullptr,
        nullptr,
        &devLinInitRecord,
        nullptr,
    },
    [](boRecord *prec) noexcept -> long {
        TRY {
            if(prec->tpro>1 || linStatDebug>=5)
                errlogPrintf("%s : DEBUG submit %s.%s\n",
                             prec->name,
                             pvt->tbl->fact.c_str(),
                             pvt->tbl->inst.c_str());

            std::weak_ptr<StatTable> wself(pvt->tbl);
            pvt->scanHandle = linStatReactor.submit([wself]() noexcept {
                if(auto job = wself.lock()) {
                    try {
                        if(linStatDebug>=5) {
                            errlogPrintf("worker execute %s.%s\n", job->fact.c_str(), job->inst.c_str());
                        }
                        job->update();
                        if(linStatDebug>=5) {
                            errlogPrintf("worker completes %s.%s\n", job->fact.c_str(), job->inst.c_str());
                        }
                        if(!job->last_ok)
                            errlogPrintf("Tbl %s:%s INFO Success\n",
                                         job->fact.c_str(),
                                         job->inst.c_str());
                        job->last_ok = true;

                    } catch(std::exception& e){
                        if(job->last_ok)
                            errlogPrintf("Tbl %s:%s " ERL_ERROR " : %s\n",
                                         job->fact.c_str(),
                                         job->inst.c_str(),
                                         e.what());
                        job->last_ok = false;
                    }
                }
            });

            return 0;
        } CATCH()
    },
};

template<typename Rec>
long devLinStatGetRVal(Rec *prec) noexcept {
    TRY {
        Guard G(pvt->tbl->lock);

        const auto& pvar = pvt->tbl->lookup(pvt->param);
        if(std::holds_alternative<std::monostate>(pvar)) {
            recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "No val");
            return 0;
        }
        const auto& param = std::get<IntVal>(pvar);

        if(prec->tse==epicsTimeEventDeviceTime) {
            prec->time = pvt->tbl->currentTime;
        }

        prec->rval = param.first;
        if(prec->mask)
            prec->rval &= prec->mask;

        return 0;
    } CATCH()
}

template<typename Rec>
long devLinStatGetVal(Rec* prec) noexcept {
    TRY {
        Guard G(pvt->tbl->lock);

        const auto& pvar = pvt->tbl->lookup(pvt->param);
        if(std::holds_alternative<std::monostate>(pvar)) {
            recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "No val");
            return 0;
        }
        const auto& param = std::get<IntVal>(pvar);

        if(prec->egu[0]=='\0' && !param.second.empty()) {
            // value has units, and EGU is empty.  Fill in automatically...
            param.second.copy(prec->egu, sizeof(prec->egu)-1);
            prec->egu[sizeof(prec->egu)-1] = '\0';
            db_post_events(prec, &prec->egu, DBE_VALUE|DBE_PROPERTY);
        }

        if(prec->tse==epicsTimeEventDeviceTime) {
            prec->time = pvt->tbl->currentTime;
        }

        prec->val = param.first;

        return 0;
    } CATCH()
}

template<int n>
constexpr dset commonReading = {
    n,
    nullptr,
    nullptr,
    &devLinInitRecord,
    &devLinTblIoIntr,
};
const aidset devLinStatAiTblTime = {
    commonReading<6>,
    [](aiRecord *prec) noexcept -> long {
        TRY {
            Guard G(pvt->tbl->lock);

            epicsTimeStamp time = pvt->tbl->currentTime;

            prec->val = time.secPastEpoch+POSIX_TIME_AT_EPICS_EPOCH + 1e-9*time.nsec;

            if(prec->tse==epicsTimeEventDeviceTime) {
                prec->time = time;
            }

            return 2;
        } CATCH()
    },
    nullptr,
};
const bidset devLinStatBIValue = {
    commonReading<5>,
    &devLinStatGetRVal<biRecord>,
};
const mbbidset devLinStatMBBIValue = {
    commonReading<5>,
    &devLinStatGetRVal<mbbiRecord>,
};
const mbbidirectdset devLinStatMBBIDirectValue = {
    commonReading<5>,
    &devLinStatGetRVal<mbbiDirectRecord>,
};
const longindset devLinStatLIValue = {
    commonReading<5>,
    &devLinStatGetVal<longinRecord>,
};
const int64indset devLinStatI64Value = {
    commonReading<5>,
    &devLinStatGetVal<int64inRecord>,
};
const stringindset devLinStatSiValue = {
    commonReading<5>,
    [](stringinRecord* prec) noexcept -> long {
        TRY {
            Guard G(pvt->tbl->lock);

            const auto& pvar = pvt->tbl->lookup(pvt->param);
            if(std::holds_alternative<std::monostate>(pvar)) {
                recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM, "No val");
                return 0;
            }
            const auto& param = std::get<std::string>(pvar);

            if(prec->tse==epicsTimeEventDeviceTime) {
                prec->time = pvt->tbl->currentTime;
            }

            auto n = param.copy(prec->val, sizeof(prec->val)-1);
            prec->val[n] = '\0';

            return 0;
        } CATCH()
    },
};

} // namespace

extern "C" {
epicsExportAddress(dset, devLinStatBoScan);
epicsExportAddress(dset, devLinStatAiTblTime);
epicsExportAddress(dset, devLinStatBIValue);
epicsExportAddress(dset, devLinStatMBBIValue);
epicsExportAddress(dset, devLinStatMBBIDirectValue);
epicsExportAddress(dset, devLinStatLIValue);
epicsExportAddress(dset, devLinStatI64Value);
epicsExportAddress(dset, devLinStatSiValue);
}
