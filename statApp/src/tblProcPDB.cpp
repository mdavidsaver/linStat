/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* information about process database
 */

#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <atomic>

#include <epicsVersion.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbServer.h>
#include <initHooks.h>
#include <dbScan.h>
// when dbServer::stats() isn't implemented...
#include <rsrv.h>
#include <taskwd.h>
#include <errlog.h>
#include <epicsThread.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "pdb";

bool markDBServStarted;

struct DBEntry {
    DBENTRY ent;
    DBEntry() {
        dbInitEntry(pdbbase, &ent);
    }
    DBEntry(const DBEntry&) = delete;
    DBEntry& operator=(const DBEntry&) = delete;
    ~DBEntry() {
        dbFinishEntry(&ent);
    }
};

struct PDBTable : public StatTable {
    uint64_t nrec=0;
    std::atomic<uint32_t> nsuspend{0};

    explicit PDBTable(const std::string& inst, const Reactor& react);
    virtual ~PDBTable();

    virtual void update() override final {
        Transaction tr(*this);

        tr.set("nrec", nrec);
        tr.set("nsuspend", nsuspend);

        if(!markDBServStarted)
            return; // first update() during init_record(), before RSRV initialized...

        {
            unsigned nchan=0, nconn=0;
#ifdef HAS_DBSERVER_STATS
            (void)dbServerStats(nullptr, &nchan, &nconn);
#else
            casStatsFetch(&nchan, &nconn);
#endif
            tr.set("rsrv:nchan", nchan);
            tr.set("rsrv:nconn", nconn);
        }

        {
            scanOnceQueueStats stats{};
            scanOnceQueueStatus(0, &stats);
            tr.set("once:size", stats.size);
            tr.set("once:used", stats.numUsed);
            tr.set("once:used:max", stats.maxUsed);
            tr.set("once:oflows", stats.numOverflow);
        }

        {
            callbackQueueStats stats;
            callbackQueueStatus(0, &stats);
            tr.set("cb:size", stats.size);

            const char * prioNames[] = {"low", "med", "high"};
            static_assert(NELEMENTS(prioNames) == NUM_CALLBACK_PRIORITIES, "");
            for(size_t i=0; i<NUM_CALLBACK_PRIORITIES; i++) {
                tr.set(SB()<<"cb:"<<prioNames[i]<<":used", stats.numUsed[i]);
                tr.set(SB()<<"cb:"<<prioNames[i]<<":used:max", stats.maxUsed[i]);
                tr.set(SB()<<"cb:"<<prioNames[i]<<":oflows", stats.numOverflow[i]);
            }
        }
    }
};

void wdnotify(void *usr, epicsThreadId tid, int suspended) {
    (void)tid;
    auto self(static_cast<PDBTable*>(usr));
    if(suspended)
        self->nsuspend++;
}

const taskwdMonitor wdactions = {
    nullptr,
    &wdnotify,
    nullptr,
};

void markDBServStart(initHookState state)
{
    if(state==initHookAfterCaServerRunning)
        markDBServStarted = true;
}

PDBTable::PDBTable(const std::string &inst, const Reactor& react)
    :StatTable(tblName, inst, react)
{
    {
        static bool registered;
        if(!registered) {
            registered = true;
            initHookRegister(markDBServStart);
        }
    }
    if(inst!="self")
        throw std::runtime_error("Only 'self' supported");

    // iterate once on startup.
    // ctor called during init_record(), after all records created
    {
        DBEntry ent;
        for(long status = dbFirstRecordType(&ent.ent);
            !status;
            status = dbNextRecordType(&ent.ent))
        {
            for(status = dbFirstRecord(&ent.ent);
                !status;
                status = dbNextRecord(&ent.ent))
            {
                if(dbIsAlias(&ent.ent))
                    continue;
                nrec++;
            }
        }
    }

    taskwdMonitorAdd(&wdactions, this);
}

PDBTable::~PDBTable() {
    taskwdMonitorDel(&wdactions, this);
}

} // namespace

DEFINE_TABLE(tblName, PDBTable)
