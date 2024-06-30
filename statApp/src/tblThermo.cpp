/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Iterate /sys/class/thermal/thermal_zone*
 */

#include <filesystem>
#include <fstream>

#include <string.h>

#include <errlog.h>
#include <epicsThread.h>

#include "linStat.h"

namespace {
using namespace linStat;
namespace fs = std::filesystem;

const char * const tblName = "thermo";

bool starts_with(const std::string& inp, const char *prefix)
{
    auto pl = strlen(prefix);
    if(inp.size() < pl)
        return false;
    return memcmp(inp.c_str(), prefix, pl)==0;
}

bool read_file(const fs::path& fname, std::string& out) {
    std::ifstream strm(fname);
    if(!strm.is_open())
        return false;

    return !!(strm>>out);
}

struct ThermoTable : public StatTable {

    const fs::path base;

    explicit ThermoTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
        ,base("/sys/class/thermal")
    {
        if(inst!="localhost")
            throw std::runtime_error("Only valid instance name is 'localhost'");
    }
    virtual ~ThermoTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        bool have_max = false;
        double Tmax = -42;
        std::string Nmax;

        for(const auto& ent : fs::directory_iterator(base)) {
            if(!ent.is_directory() || !starts_with(ent.path().filename().string(), "thermal_zone"))
                continue;

            auto tfile(ent.path() / "temp");
            std::ifstream strm(tfile);
            std::string tstr, name;

            if(!read_file(ent.path() / "temp", tstr) || !read_file(ent.path() / "type", name))
                continue;

            int32_t tval;
            try{
                tval = std::stol(tstr);
            }catch(std::exception&){
                if(linStatDebug>0)
                    errlogPrintf("%s.%s : error parsing '%s'\n",
                                 fact.c_str(),
                                 inst.c_str(),
                                 tstr.c_str());
                continue;
            }

            if(!have_max || tval > Tmax) {
                Tmax = tval;
                Nmax = name;
                have_max = true;
            }
        }

        if(have_max) {
            tr.set("temp_max", Tmax, "mC"); // 1/1000 deg. C
            tr.set("name_max", Nmax);
        }
    }
};


} // namespace

DEFINE_TABLE(tblName, ThermoTable)
