/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* call mallinfo2()
 */

#include <malloc.h>

#include <dbDefs.h>
#include <errlog.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "mallinfo";

struct MallInfoTable : public StatTable {

    explicit MallInfoTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {
        if(inst!="self")
            throw std::runtime_error("Only 'self' supported");
    }
    virtual ~MallInfoTable() {}

    virtual void update() override final {
        Transaction tr(*this);

#if GLIBC_VERSION >= VERSION_INT(2, 33, 0, 0)
        auto info(mallinfo2());
#else
        auto info(mallinfo());
#endif
#define CASE(NAME) tr.set(#NAME, info.NAME)
        CASE(arena);
        CASE(ordblks);
        //CASE(smblks); // unused
        CASE(hblks);
        CASE(hblkhd);
        //CASE(usmblks); // unused
        //CASE(fsmblks); // unused
        CASE(uordblks);
        CASE(fordblks);
        CASE(keepcost);
#undef CASE
    }
};

} // namespace

DEFINE_TABLE(tblName, MallInfoTable)
