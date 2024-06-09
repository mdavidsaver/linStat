/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read from /proc/stat
 */

#include <fstream>
#include <regex>

#include <unistd.h>

#include <epicsString.h>
#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "stat";

struct SysStatTable : public StatTable {
    bool success = true;

    explicit SysStatTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {
        if(inst!="localhost")
            throw std::runtime_error("Only valid instance name is 'localhost'");
    }
    virtual ~SysStatTable() {}

    virtual void update() override final {
        Transaction tr(*this);
        std::ifstream strm("/proc/stat");
        if(!strm.is_open()) {
            if(success) {
                errlogPrintf("Tbl %s.%s " ERL_ERROR " Unable to open: /proc/stat\n",
                             fact.c_str(),
                             inst.c_str());
            }
            success = false;
            return;
        }

        std::string line;
        while(std::getline(strm, line)) {
            if(line.substr(0,4)=="cpu ") {
                char *save = nullptr;
                unsigned idx=2; // columns indexed from 2 (1 is "cpu")
                for(const char *ent = epicsStrtok_r(line.data() + 4, " ", &save);
                    ent;
                    ent = epicsStrtok_r(nullptr, " ", &save), idx++)
                {
                    try {
                        tr.set(SB()<<"cpu:"<<idx, std::stoul(ent));
                    }catch(std::exception& e){
                        if(linStatDebug>=3)
                            errlogPrintf(ERL_ERROR " %s Unable to parse:%u \"%s\"\n",
                                         __func__,
                                         idx,
                                         ent);
                    }
                }
                break; // TODO: process other lines?
            }
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, SysStatTable)
