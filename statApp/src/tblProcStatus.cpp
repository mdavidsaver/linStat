/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read from /proc/<pid>/status
 */

#include <fstream>
#include <regex>
#include <set>
#include <string>

#include <unistd.h>

#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "procStatus";

struct ProcStatusTable : public StatTable {
    const std::string fname; // "/proc/<pid>/status"
    bool success = true;

    // special cases where the value is hex without an "0x" prefix.
    const std::set<std::string> bitmaps;

    explicit ProcStatusTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
        ,fname("/proc/self/status")
        ,bitmaps({
                 "SigPnd",
                 "ShdPnd",
                 "SigBlk",
                 "SigIgn",
                 "SigCgt",
                 "CapInh",
                 "CapEff",
                 "CapBnd",
                 "CapAmb",
                 "Cpus_allowed",
        })
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
            /* /proc/<pid>/status has a whole zoo of different value formats.
             *
             * Match lines like:
             *   Key: <string>
             *   Key: <number>
             *   Key: <number> <unit>
             *
             * Where number is decimal or hex (sometimes without "0x")
             *
             * Not matching like:
             *   Key: <some string>
             *   Key: <number> <number> ...
             *   Key: <number>,<number>,...
             *   Key: <number>-<number>
             *   Key: <number>/<number>
             */
            // (key) : (num) (unit)? | (num) (ignore)
            static const std::regex expr(
                        R"(^([^:]+):\s*(?:0x)?([0-9a-fA-F]+)\s*([a-zA-Z]+)?\s*$$)",
                        std::regex_constants::optimize);

            std::smatch M;
            if(!std::regex_match(line, M, expr)) {
                if(linStatDebug>=3)
                    errlogPrintf(ERL_ERROR " Unable to parse:%s\n", line.c_str());
                continue;
            }

            auto key(M[1].str());
            bool reallyhex = bitmaps.count(key)>0;
            int64_t val;

            try {
                val = std::stol(M[2].str(), nullptr, reallyhex ? 16 : 0);
            } catch(std::exception& e) {
                if(linStatDebug>=3)
                    errlogPrintf(ERL_ERROR " Unable to parse %s number:%s\n",
                                 key.c_str(), M[2].str().c_str());
                continue;
            }

            tr.set(key,
                   val,
                   M[3].matched ? M[3].str() : std::string());
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, ProcStatusTable)
