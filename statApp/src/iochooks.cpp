/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define USE_TYPED_DRVET

#include <drvSup.h>
#include <errlog.h>
#include <initHooks.h>
#include <epicsExit.h>
#include <epicsStdio.h>
#include <epicsExport.h>

#include "linStat.h"

namespace linStat {
int linStatDebug = 0;
Reactor linStatReactor;
} // namespace linStat

namespace {
using namespace linStat;

long linStatReport(int level) noexcept {
    try {
        if(level<=0)
            return 0;

        for(auto fact : StatTableIter()) {
            if(fact.name)
                epicsPrintf("  Factory %s : %p\n", fact.name, fact.create);
        }

        if(level<=2)
            return 0;

        for(const auto& tbl : StatTable::list_all()) {
            epicsPrintf("  Table: %s.%s\n", tbl->fact.c_str(), tbl->inst.c_str());
            Guard G(tbl->lock);
            for(const auto& pr : tbl->current) {
                if(auto val = std::get_if<IntVal>(&pr.second)) {
                    epicsPrintf("    int64_t %s = %lld %s\n",
                                pr.first.c_str(),
                                (long long)val->first,
                                val->second.c_str());
                } else if(auto val = std::get_if<std::string>(&pr.second)) {
                    epicsPrintf("    string %s = %s\n",
                                pr.first.c_str(),
                                val->c_str());
                } else {
                    epicsPrintf("    ?????? %s\n",
                                pr.first.c_str());
                }
            }
        }

        return 0;
    }catch(std::exception& e){
        epicsPrintf(ERL_ERROR " : %s\n", e.what());
        return -1;
    }
}

const drvet drvLinStat = {
    2,
    &linStatReport,
    nullptr,
};

void linStatStart(initHookState state) noexcept {
    try{
        if(state==initHookAfterIocRunning)
            linStatReactor.start();
    }catch(std::exception& e){
        fprintf(epicsGetStderr(), "%s : " ERL_ERROR " %s\n", __func__, e.what());
    }
}

void linStatStop(void*) noexcept {
    try{
        linStatReactor.stop();
    }catch(std::exception& e){
        fprintf(epicsGetStderr(), "%s : " ERL_ERROR " %s\n", __func__, e.what());
    }
}

void linStatRegistrar() noexcept {
    try{
        linStatReactor = Reactor::create();
        initHookRegister(&linStatStart);
        epicsAtExit(&linStatStop, nullptr);
    }catch(std::exception& e){
        fprintf(epicsGetStderr(), "%s : " ERL_ERROR " %s\n", __func__, e.what());
    }
}

} // namespace

extern "C" {
epicsExportAddress(drvet, drvLinStat);
epicsExportRegistrar(linStatRegistrar);
epicsExportAddress(int, linStatDebug);
}
