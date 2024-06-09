/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read from /proc/<pid>/stat
 */

#include <fstream>
#include <regex>
#include <string>

#include <unistd.h>

#include <errlog.h>
#include <epicsString.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "procStat";

struct ProcStatTable : public StatTable {
    const std::string fname; // "/proc/<pid>/stat"
    bool success = true;

    explicit ProcStatTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
        ,fname("/proc/self/stat")
    {
        if(inst!="self")
            throw std::runtime_error("Unsupported process name");
    }
    virtual ~ProcStatTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        std::ifstream strm(fname);
        if(!strm.is_open()) {
            if(success) {
                errlogPrintf("Tbl %s.%s " ERL_ERROR " Unable to open: %s\n",
                             fact.c_str(),
                             inst.c_str(),
                             fname.c_str());
            }
            success = false;
            return;
        }

        std::string line;
        if(!std::getline(strm, line)) { // read first, and so far only, line
            if(linStatDebug>=3)
                errlogPrintf(ERL_ERROR " %s No line\n", __func__);
        }
        // 44558 (kwrite) R ... followed by space seperated numbers

        static const std::regex expr(R"((\d+)\s+\(((?:[^)]|\\.)*)\)\s+([A-Z]+)\s+(.*))");

        std::smatch M;
        if(!std::regex_match(line, M, expr)) {
            if(linStatDebug>=3)
                errlogPrintf(ERL_ERROR " %s Unable to parse:%s\n",
                             __func__,
                             line.substr(0, 20).c_str());
            return;
        }
        assert(M.size()==5);

        // parameter "names" are 1-index
        tr.set("1", std::stol(M[1].str()));
        tr.set("2", M[2].str());
        tr.set("3", M[3].str());
        unsigned idx = 4;

        char *save = nullptr;
        for(const char *ent = epicsStrtok_r(line.data() + M.position(4), " ", &save);
            ent;
            ent = epicsStrtok_r(nullptr, " ", &save), idx++)
        {
            try{
                tr.set(std::to_string(idx), std::stoul(ent));
            } catch(std::exception& e){
                if(linStatDebug>=3)
                    errlogPrintf(ERL_ERROR " %s Unable to parse:%u \"%s\"\n",
                                 __func__,
                                 idx,
                                 ent);
            }

        }
    }
};

} // namespace

DEFINE_TABLE(tblName, ProcStatTable)
