/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read from /proc/meminfo
 */

#include <fstream>
#include <regex>

#include <unistd.h>

#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "memInfo";

struct MemInfoTable : public StatTable {

    explicit MemInfoTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {}
    virtual ~MemInfoTable() {}

    virtual void update() override final {
        Transaction tr(*this);
        std::ifstream strm("/proc/meminfo");
        if(!strm.is_open()) {
            errlogPrintf("Tbl %s.%s " ERL_ERROR " Unable to open: /proc/meminfo\n",
                         fact.c_str(),
                         inst.c_str());
            return;
        }

        std::string line;
        while(std::getline(strm, line)) {
            // lines like:
            //   Key: <number>
            //   Key: <number> <unit>
            //   ...
            static const std::regex expr(R"(^([^:]+):\s*([0-9]+)(?:\s*([^ \t]+))?\s*$)");

            std::smatch M;
            if(!std::regex_match(line, M, expr)) {
                if(linStatDebug>=3)
                    errlogPrintf("Unable to parse:%s\n", line.c_str());
                continue;
            }
            assert(M.size()==4);
            auto key(M[1].str());
            auto val(M[2].str());
            std::string egu;
            if(M[3].matched)
                egu = M[3].str();

            tr.set(key, std::stol(val), egu);
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, MemInfoTable)
