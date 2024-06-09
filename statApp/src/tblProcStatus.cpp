/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read from /proc/<pid>/status
 */

#include <fstream>
#include <regex>

#include <unistd.h>

#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "procStatus";

struct ProcStatusTable : public StatTable {
    const std::string fname; // "/proc/<pid>/status"
    bool success = true;

    explicit ProcStatusTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
        ,fname("/proc/self/status")
    {
        if(inst!="self")
            throw std::runtime_error("Unsupported process name");
    }
    virtual ~ProcStatusTable() {}

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
        while(std::getline(strm, line)) {
            // lines like:
            //   Key: <string>
            //   Key: <number>
            //   Key: <number> <unit>
            //   ...
            static const std::regex expr(R"(^([^:]+):\s*(?:([0-9]+)(?:\s*([^ \t]+))?|([^0-9][^ \t]*))\s*$)");

            std::smatch M;
            if(!std::regex_match(line, M, expr)) {
                if(linStatDebug>=3)
                    errlogPrintf(ERL_ERROR " Unable to parse:%s\n", line.c_str());
                continue;
            }
            assert(M.size()==5);

            auto key(M[1].str());
            if(M[2].matched) { // integer
                std::string egu;
                if(M[3].matched)
                    egu = M[3].str();
                tr.set(key, std::stol(M[2].str()), egu);

            } else if(M[4].matched) {
                tr.set(key, M[4].str());
            }
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, ProcStatusTable)
