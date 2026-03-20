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
