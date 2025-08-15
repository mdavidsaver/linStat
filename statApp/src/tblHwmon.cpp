/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Iterate /sys/class/hwmon/hwmon*
 *   .../name
 *       temp*_label
 *       temp*_input
 *       fan*_label
 *       fan*_input
 *       fan*_target
 *
 * https://www.kernel.org/doc/Documentation/hwmon/sysfs-interface
 */

#include <filesystem>
#include <set>
#include <fstream>
#include <regex>

#include <string.h>

#include <errlog.h>
#include <epicsThread.h>

#include "linStat.h"

namespace {
using namespace linStat;
namespace fs = std::filesystem;

const char * const tblName = "hwmon";

struct HWMonTable : public StatTable {

    const fs::path base;

    explicit HWMonTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
        ,base("/sys/class/hwmon")
    {
        if(inst!="localhost")
            throw std::runtime_error("Only valid instance name is 'localhost'");
    }
    virtual ~HWMonTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        std::set<std::string> devs_seen;

        bool have_Tmax = false;
        int32_t Tmax = -42;
        std::string Nmax;

        // iterate through hwmon* devices
        for(const auto& ent : fs::directory_iterator(base)) {
            if(!ent.is_directory() || !starts_with(ent.path().filename().string(), "hwmon"))
                continue;

            std::string dev;
            if(!read_file(ent.path() / "name", dev) || dev.empty())
                continue;

            if(devs_seen.find(dev)!=devs_seen.end())
                continue; // oops, duplicate monitor device name
            devs_seen.insert(dev);

            // iterate through and group parameters by sensor
            std::map<std::string, std::pair<std::string, int32_t>> temperatures;

            for(const auto& sv : fs::directory_iterator(ent.path())) {
                // looking for entries of the form: <type><inst#>_<param>
                static const std::regex expr(R"(^([a-z]+)(\d+)_([a-z]+)$)");

                std::smatch M;
                {
                    auto fname(sv.path().filename().string());
                    if(!std::regex_match(fname, M, expr))
                        continue;
                }

                auto type(M[1].str());
                auto inst(M[2].str());
                auto param(M[3].str());

                std::string value;
                if(!read_file(sv.path(), value))
                    continue;

                if(type=="temp") {
                    if(param=="label") {
                        temperatures[SB()<<type<<inst].first = std::move(value);
                    } else if(param=="input") {
                        try{
                            temperatures[SB()<<type<<inst].second = std::stol(value);
                        }catch(std::exception&){
                            if(linStatDebug>0)
                                errlogPrintf("%s.%s : error parsing '%s'\n",
                                             fact.c_str(),
                                             inst.c_str(),
                                             value.c_str());
                        }
                    }
                }
            }

            std::map<std::string, unsigned> label_counts;
            for(const auto& t : temperatures) {
                auto label(t.second.first);
                if(label.empty())
                    label = t.first;

                auto count(label_counts[label]++);

                tr.set(SB()<<dev<<'.'<<label<<'.'<<count<<".label", SB()<<label<<" ["<<count<<']');
                tr.set(SB()<<dev<<'.'<<label<<'.'<<count<<".temp", t.second.second, "mC"); // 1/1000 deg. C

                if(!have_Tmax || t.second.second > Tmax) {
                    have_Tmax = true;
                    Tmax = t.second.second;
                    Nmax = SB()<<dev<<'.'<<label<<'.'<<count;
                }
            }

        }

        if(have_Tmax) {
            tr.set("temp_max", Tmax, "mC"); // 1/1000 deg. C
            tr.set("name_max", Nmax);
        } else {
            tr.set("temp_max");
            tr.set("name_max");
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, HWMonTable)
