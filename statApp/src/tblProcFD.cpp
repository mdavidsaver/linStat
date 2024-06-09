/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* iterate /proc/self/fd/
 */

#define _LARGEFILE64_SOURCE

#include <vector>
#include <memory>

#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <dbDefs.h>
#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

struct DIRcloser {
    void operator()(DIR *dir) const {
        if(dir)
            (void)closedir(dir);
    }
};

const char * const tblName = "fd";

struct FDTable : public StatTable {

    explicit FDTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {
        if(inst!="self")
            throw std::runtime_error("Only 'self' supported");
    }
    virtual ~FDTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        std::unique_ptr<DIR, DIRcloser> idir{opendir("/proc/self/fd")};
        if(!idir) {
            if(linStatDebug>=3)
                errlogPrintf(ERL_ERROR " Unable to open /proc/self/fd\n");
            return;
        }

        uint64_t nFD = 0;

        while(true) {
            errno = 0; // man readdir recommends to distinguish normal end of iteration
            auto ent = readdir(idir.get());
            if(!ent)
                break;
            else if(strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0)
                continue;

            nFD++;
        }

        if(errno==0) {
            tr.set("FD_CNT", nFD);

        } else if(linStatDebug>=3) {
                errlogPrintf(ERL_ERROR " Unable to open /proc/self/fd\n");
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, FDTable)
