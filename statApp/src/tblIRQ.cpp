/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
// process /proc/interrupts

#include <fstream>
#include <string>
#include <regex>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "irq";

struct IRQTable : public StatTable {


    explicit IRQTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {}
    virtual ~IRQTable() {}

    virtual void update() override final {
        Transaction tr(*this);
        std::ifstream strm("/proc/interrupts");
        if(!strm.is_open()) {
            errlogPrintf("Tbl %s.%s " ERL_ERROR " Unable to open: /proc/interrupts\n",
                         fact.c_str(),
                         inst.c_str());
            return;
        }

        // first line like:
        // "   CPU0  CPU1 ... CPUN"
        std::string line;
        if(!std::getline(strm, line))
            return;

        static const std::regex nonspace(R"([^\s]+)");

        size_t ncpu=0;
        for(std::sregex_iterator it(line.begin(), line.end(), nonspace), end; it!=end; ++it) {
            ncpu++;
        }

        size_t total_irq = 0u;

        while(std::getline(strm, line)) {
            // remaining lines like:
            // " <src>:  <cpu0#> ... <cpuN#> <SRC> <TYPE> <dev0> [, <dev1, ...]"
            // expect numbered hardware interrupts first, then named special interrupts (eg. "NMI")

            size_t ncol=0;
            for(std::sregex_iterator it(line.begin(), line.end(), nonspace), end; it!=end; ++it, ncol++) {
                auto col = it->str();
                if(ncol==0) { // interrupt number/name
                    if(col[0]>='0' && col[0]<='9') {
                        // number
                        continue;
                    } else {
                        // named
                        break; // stop
                    }

                } else if(ncol==ncpu+2) { // <SRC>
                    break; // next line
                }

                size_t nirq;
                try{
                    nirq = std::stoul(col);

                }catch(std::exception& e) {
                    if(linStatDebug>0)
                        errlogPrintf("%s.%s : error parsing '%s'\n",
                                     fact.c_str(),
                                     inst.c_str(),
                                     col.c_str());
                    continue;
                }
                total_irq += nirq;
            }
        }

        tr.set("total_irq", total_irq);
    }
};

} // namespace

DEFINE_TABLE(tblName, IRQTable)
