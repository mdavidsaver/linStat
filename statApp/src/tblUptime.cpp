/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* read /proc/uptime
 */

#include <vector>
#include <regex>

#include <math.h>

#include <dbDefs.h>
#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "uptime";

struct Interval {
    uint64_t period; // seconds
    const char *label;
};
const Interval intervals[] = { // in decending order
    {60*60*24, "D"},
    {60*60, "H"},
    {60, "M"},
};

struct UptimeTable : public StatTable {

    explicit UptimeTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {
        if(inst!="localhost")
            throw std::runtime_error("Only valid instance name is 'localhost'");
    }
    virtual ~UptimeTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        std::ifstream strm("/proc/uptime");
        if(!strm.is_open()) {
            if(linStatDebug>=3) {
                errlogPrintf("Tbl %s.%s " ERL_ERROR " Unable to open: /proc/uptime\n",
                             fact.c_str(),
                             inst.c_str());
            }
            return;
        }

        std::string line;
        if(!std::getline(strm, line)) { // read first, and so far only, line
            if(linStatDebug>=3)
                errlogPrintf(ERL_ERROR " %s No line\n", __func__);
        }
        // 516577.42 1980838.26

        static const std::regex expr(R"(([0-9.]+)\s+([0-9.]+)(?:\s.*)?)");

        std::smatch M;
        if(!std::regex_match(line, M, expr)) {
            if(linStatDebug>=3)
                errlogPrintf(ERL_ERROR " %s Unable to parse:%s\n",
                             __func__,
                             line.substr(0, 20).c_str());
            return;
        }
        assert(M.size()==3);

        auto uptime(std::stod(M[1].str()));
        tr.set("uptime", uptime, "s");

        std::ostringstream msg;
        for(size_t i=0; i<NELEMENTS(intervals); i++) {
            const auto& ivl = intervals[i];
            if(auto cnt = (unsigned long)(uptime / ivl.period)) {
                msg<<cnt<<" "<<ivl.label<<" ";
                uptime = fmod(uptime, ivl.period);
            }
        }
        msg<<(unsigned long)uptime<<" S";
        tr.set("uptime:disp", msg.str());
    }
};

} // namespace

DEFINE_TABLE(tblName, UptimeTable)
