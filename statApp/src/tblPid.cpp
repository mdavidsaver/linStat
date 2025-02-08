/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* misc. process specific.
 *   /proc/self/exe
 *   getcwd()
 *   getrlimit()  (aka. ulimit)
 */

#define _LARGEFILE64_SOURCE

#include <vector>

#include <unistd.h>
#include <sys/resource.h>

#include <dbDefs.h>
#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "pid";

struct limited {
    int resource;
    const char* key;
};
const limited rlimits[] = {
    #define CASE(NAME) {RLIMIT_ ## NAME, #NAME}
    CASE(CORE),
    CASE(MEMLOCK),
    CASE(NICE),
    CASE(NOFILE),
    CASE(NPROC),
    CASE(RTPRIO),
    CASE(RTTIME),
};

struct PidTable : public StatTable {
    std::vector<char> scratch;
    long ticks_per_second;

    explicit PidTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
        ,scratch(MAX_STRING_SIZE)
        ,ticks_per_second(sysconf(_SC_CLK_TCK))
    {
        if(inst!="self")
            throw std::runtime_error("Only 'self' supported");
    }
    virtual ~PidTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        tr.set("ticks_per_sec", ticks_per_second);

        while(true) {
            auto ret = readlink("/proc/self/exe", scratch.data(), scratch.size()-1);
            if(ret<0) {
                scratch[0] = '\0';
                break;
            } else if(size_t(ret)>=scratch.size()-1) {
                scratch.resize(scratch.size()*2);
            } else {
                scratch[ret] = '\0';
                tr.set("exe", scratch.data());
                break;
            }
        }

        while(true) {
            if(getcwd(scratch.data(), scratch.size()-1)) {
                scratch.back() = '\0';
                tr.set("cwd", scratch.data()); // full path
                std::string cwd(scratch.data());
                // break up long paths based on stringin limitation
                tr.set("cwd1", cwd.substr(0, 39).data());
                if(cwd.size()>39)
                    tr.set("cwd2", cwd.substr(39, 39*2).data());
                else
                    tr.set("cwd2", "");
                break;
            } else if(errno==ERANGE) {
                scratch.resize(scratch.size()*2);
            } else {
                scratch[0]=0;
                break;
            }
        }

        for(size_t idx = 0; idx<NELEMENTS(rlimits); idx++) {
            auto res = rlimits[idx];
            struct rlimit64 lim{};
            if(int err = getrlimit64(res.resource, &lim)) {
                if(linStatDebug>=3)
                    errlogPrintf(ERL_ERROR " Unable to getrlimit %s : %d\n", res.key, err);
            } else {
                tr.set(res.key, lim.rlim_cur);
                tr.set(SB()<<res.key<<":MAX", lim.rlim_max);
            }
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, PidTable)
