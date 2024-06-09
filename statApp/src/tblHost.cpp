/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read from uname() and other misc. system-wide information
 */

#include <unistd.h>
#include <sys/utsname.h>

#include <errlog.h>
#include <epicsThread.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "host";

struct HostTable : public StatTable {

    explicit HostTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {
        if(inst!="localhost")
            throw std::runtime_error("Only valid instance name is 'localhost'");
    }
    virtual ~HostTable() {}

    virtual void update() override final {
        Transaction tr(*this);
        {
            utsname info;
            if(uname(&info)) {
                errlogPrintf(ERL_ERROR " : uname() %d\n", errno);
            } else {
                tr.set("sysname", info.sysname);
                tr.set("nodename", info.nodename);
                tr.set("release", info.release);
                tr.set("version", info.version);
                tr.set("machine", info.machine);
            }
        }
        tr.set("ncpu", epicsThreadGetCPUs());
    }
};


} // namespace

DEFINE_TABLE(tblName, HostTable)
