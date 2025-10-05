/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
// process /proc/interrupts

#include <map>
#include <fstream>
#include <string>
#include <regex>
#include <stdexcept>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "irq";

struct IRQTable : public StatTable {
    // label -> count
    std::map<std::string, size_t> prev;

    explicit IRQTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {}
    virtual ~IRQTable() {}

    decltype(prev) readirq() {
        decltype(prev) counts;
        std::ifstream strm("/proc/interrupts");
        if(!strm.is_open()) {
            throw std::runtime_error("Unable to open: /proc/interrupts");
        }

        // first line like:
        // "   CPU0  CPU1 ... CPUN"
        std::string line;
        if(!std::getline(strm, line)) {
            throw std::runtime_error("Unexpected empty: /proc/interrupts");
        }

        static const std::regex nonspace(R"([^\s]+)");

        size_t ncpu=0;
        for(std::sregex_iterator it(line.begin(), line.end(), nonspace), end; it!=end; ++it) {
            ncpu++;
        }
        if(ncpu==0)
            throw std::runtime_error("Found no CPUS?");

        while(std::getline(strm, line)) {
            // remaining lines like:
            // " <src>:  <cpu0#> ... <cpuN#> <SRC> <TYPE> <dev0> [, <dev1, ...]"
            // expect numbered hardware interrupts first, then named special interrupts (eg. "NMI")

            size_t n_irq = 0u;
            std::ostringstream label;

            size_t ncol=0;
            for(std::sregex_iterator it(line.begin(), line.end(), nonspace), end; it!=end; ++it, ncol++) {
                auto col = it->str();
                if(ncol==0) { // <src>:
                    label<<col;

                } else if(ncol < ncpu+1) { // <cpu0#> ... <cpuN#>
                    size_t num;
                    try{
                        num = std::stoul(col);

                    }catch(std::exception& e) {
                        if(linStatDebug>0)
                            errlogPrintf("%s.%s : error parsing #%zu '%s'\n",
                                         fact.c_str(),
                                         inst.c_str(),
                                         ncol,
                                         col.c_str());
                        continue;
                    }
                    n_irq += num;

                } else { // <SRC> <TYPE> <dev0> [, <dev1, ...]"
                    label<<' '<<col;
                }
            }

            counts[label.str()] = n_irq;
        }

        return counts;
    }

    virtual void update() override final {
        Transaction tr(*this);

        auto counts(readirq());
        size_t total_irq=0u, max_delta=0u;
        std::string max_name;

        for(const auto& p : counts) {
            auto delta = p.second;
            total_irq += delta;

            auto pit(prev.find(p.first));
            if(pit!=prev.end())
                delta -= pit->second;

            if(delta > max_delta) {
                max_delta = delta;
                max_name = p.first;
            }
        }

        auto deltT = tr.nextTime - currentTime;

        tr.set("total_irq", total_irq);
        tr.setf("max_irq", max_delta/deltT, "Hz");
        tr.set("max_irq_name", max_name);
        prev = counts;
    }
};

} // namespace

DEFINE_TABLE(tblName, IRQTable)
