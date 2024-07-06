/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read sequence-of-tables format use for:
 * - /proc/net/netstat
 * - /proc/net/snmp
 * - /proc/self/net/netstat
 * - /proc/self/net/snmp
 */

#include <errlog.h>
#include <epicsThread.h>
#include <epicsString.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "netstat";

struct NetstatTable : public StatTable {
    std::string fname;
    std::vector<std::string> labels; // persist to avoid re-alloc

    explicit NetstatTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
        ,fname(inst)
    {}
    virtual ~NetstatTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        std::ifstream strm(fname);
        if(!strm.is_open()) {
            errlogPrintf("Tbl %s.%s " ERL_ERROR " Unable to open: %s\n",
                         fact.c_str(),
                         inst.c_str(),
                         fname.c_str());
            return;
        }

        std::string table; // doubles as parser state.  empty == expect header, !empty == expect values

        std::string line;
        while(std::getline(strm, line)) {
            // consecutive pairs of lines like:
            //   Tbl: Label1 Label2 ...
            //   Tbl: Value1 Value2 ...

            auto sep = line.find_first_of(':');
            if(!sep)
                continue; // not likely...

            auto name(line.substr(0, sep));
            if(table.empty()) {
                labels.clear();

                char *save = nullptr;
                for(const char *ent = epicsStrtok_r(line.data() + sep+1, " ", &save);
                    ent;
                    ent = epicsStrtok_r(nullptr, " ", &save))
                {
                    labels.push_back(ent);
                }
                table = name;

            } else if(table!=name) {
                errlogPrintf("Tbl %s.%s " ERL_ERROR " mis-matched tables %s != %s\n",
                             fact.c_str(),
                             inst.c_str(),
                             name.c_str(),
                             table.c_str());
                break;

            } else {
                auto it = labels.begin();
                char *save = nullptr;
                for(const char *ent = epicsStrtok_r(line.data() + sep+1, " ", &save);
                    ent && it!=labels.end();
                    ent = epicsStrtok_r(nullptr, " ", &save), ++it)
                {
                    uint64_t val;
                    try {
                        val = std::stoul(ent);
                    } catch(std::exception& e){
                        if(linStatDebug>=3)
                            errlogPrintf(ERL_ERROR " %s Unable to parse:%s \"%s\"\n",
                                         __func__,
                                         it->c_str(),
                                         ent);
                        continue;
                    }

                    tr.set(SB()<<table<<":"<<(*it), val);
                }
                table.clear();
            }
        }

    }
};


} // namespace

DEFINE_TABLE(tblName, NetstatTable)
