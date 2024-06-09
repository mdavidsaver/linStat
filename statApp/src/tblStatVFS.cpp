/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define _LARGEFILE64_SOURCE

#include <vector>

#include <sys/statvfs.h>

#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "statvfs";

struct StatVFSTable : public StatTable {
    bool success = true;

    explicit StatVFSTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {}
    virtual ~StatVFSTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        struct statvfs64 tmp{};
        if(0==statvfs64(inst.c_str(), &tmp)) {
            tr.set("size", tmp.f_frsize * tmp.f_blocks, "B");
            tr.set("avail", tmp.f_bsize * tmp.f_bavail, "B");
            tr.set("isize", tmp.f_files, "files");
            tr.set("iavail", tmp.f_favail, "files");
            if(!success)
                errlogPrintf("INFO able to stat \"%s\"\n", inst.c_str());
            success = true;
        } else {
            if(success)
                errlogPrintf(ERL_ERROR " unable to stat \"%s\"\n", inst.c_str());
            success = false;
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, StatVFSTable)
