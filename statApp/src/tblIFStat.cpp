/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read from uname() and other misc. system-wide information
 */

#include <unistd.h>
#include <sys/utsname.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_link.h>

#include <errlog.h>
#include <epicsThread.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "ifstat";

struct IFStatTable : public StatTable {
    Handle op;
    std::vector<char> scratch;

    explicit IFStatTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {
        if(inst.empty())
            throw std::runtime_error("Empty instance not allowed");

        // pre-construct request message

        // pieces of NLMSG need to be aligned to 4 bytes
        static_assert(sizeof(nlmsghdr)%4==0);
        static_assert(sizeof(ifinfomsg)%4==0);
        scratch.resize(sizeof(nlmsghdr)
                       + sizeof(ifinfomsg)
                       + RTA_SPACE(inst.size()+1)); // IF name needs trailing nil

        auto cur =  scratch.data();
        auto msg = reinterpret_cast<nlmsghdr*>(cur);
        cur += sizeof(*msg);
        auto ifinfo = reinterpret_cast<ifinfomsg*>(cur);
        cur += sizeof(*ifinfo);
        auto ifname = reinterpret_cast<rtattr*>(cur);

        msg->nlmsg_len = scratch.size();
        msg->nlmsg_type = RTM_GETLINK;
        msg->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

        // leave ifinfomsg zeroed

        ifname->rta_type = IFLA_IFNAME;
        ifname->rta_len = RTA_LENGTH(inst.size()+1);
        inst.copy((char*)RTA_DATA(ifname), inst.size());

    }
    virtual ~IFStatTable() {}

    void digest_newlink(const nlmsghdr *msg) {
        Transaction tr(*this);
        tr.set("name", inst);

        auto pos = (const char*)NLMSG_DATA(msg);
        auto remaining = msg->nlmsg_len - NLMSG_HDRLEN;

        const auto ifinfo = reinterpret_cast<const ifinfomsg*>(pos);

        tr.set("if_flags", ifinfo->ifi_flags); // IFF_* defined in linux/if.h

        pos += sizeof(ifinfomsg); // skip
        remaining -= sizeof(ifinfomsg);

        for(auto ratt = reinterpret_cast<const rtattr*>(pos);
            RTA_OK(ratt, remaining);
            ratt = RTA_NEXT(ratt, remaining))
        {
#define XRTA_CHECK(TYPE, ENUM, ATTR) \
    (( (ATTR)->rta_type == ENUM && RTA_PAYLOAD(ATTR)>=sizeof(TYPE) ? \
    reinterpret_cast<const TYPE*>(RTA_DATA(ATTR)) : 0 ))

            if(auto stat = XRTA_CHECK(rtnl_link_stats64, IFLA_STATS64, ratt)) {
#define CASE(NAME) tr.set(#NAME, stat->NAME)
                CASE(rx_packets);
                CASE(tx_packets);
                CASE(rx_bytes);
                CASE(tx_bytes);
                CASE(rx_errors);
                CASE(tx_errors);
                CASE(rx_dropped);
                CASE(tx_dropped);
                CASE(multicast);
                CASE(collisions);
                CASE(rx_length_errors);
                CASE(rx_over_errors);
                CASE(rx_crc_errors);
                CASE(rx_frame_errors);
                CASE(rx_fifo_errors);
                CASE(rx_missed_errors);
                CASE(tx_aborted_errors);
                CASE(tx_carrier_errors);
                CASE(tx_fifo_errors);
                CASE(tx_heartbeat_errors);
                CASE(tx_window_errors);
                CASE(rx_compressed);
                CASE(tx_compressed);
                CASE(rx_nohandler);
                // TODO: test linux header version for support of:
                //CASE(rx_otherhost_dropped);

#undef CASE

            } else if(auto body = XRTA_CHECK(uint8_t, IFLA_OPERSTATE, ratt)) {
                tr.set("operatate", *body);

            } else if(auto body = XRTA_CHECK(uint8_t, IFLA_CARRIER, ratt)) {
                tr.set("carrier", *body);

            } else if(auto body = XRTA_CHECK(uint8_t, IFLA_LINKMODE, ratt)) {
                tr.set("linkmode", *body);

            } else if(auto body = XRTA_CHECK(uint32_t, IFLA_CARRIER_UP_COUNT, ratt)) {
                tr.set("carrier_up_count", *body);

            } else if(auto body = XRTA_CHECK(uint32_t, IFLA_CARRIER_DOWN_COUNT, ratt)) {
                tr.set("carrier_down_count", *body);

            } else if(auto body = XRTA_CHECK(uint32_t, IFLA_MTU, ratt)) {
                tr.set("mtu", *body);

            } else if(linStatDebug>=5) {
                errlogPrintf("%s.%s ignore attr %hu size %zu\n",
                             fact.c_str(), inst.c_str(),
                             ratt->rta_type,
                             ratt->rta_len - RTA_LENGTH(0));
            }
        }

    }

    virtual void update() override final {
        if(op) {
            if(linStatDebug>=5)
                errlogPrintf("%s.%s INFO busy\n",
                             fact.c_str(), inst.c_str());
            return;
        }

        auto raw(std::make_shared<std::vector<char>>(scratch)); // copy
        NLMsg msg(raw, (nlmsghdr*)raw->data());
        op = react.request(std::move(msg), [this](NLMsg&& rep){
            if(rep->nlmsg_type==RTM_NEWLINK && rep->nlmsg_len >= NLMSG_HDRLEN+sizeof(ifinfomsg)) {
                // TODO: handle possibility of multi-part reply for a single interface?
                digest_newlink(rep.get());

            } else if(rep->nlmsg_type==NLMSG_ERROR && rep->nlmsg_len >= NLMSG_HDRLEN+sizeof(nlmsgerr)) {
                op.reset();
                const auto err = reinterpret_cast<nlmsgerr*>(NLMSG_DATA(rep.get()));
                if(err->error) {
                    errlogPrintf("%s.%s " ERL_ERROR " query fails : %d\n",
                                 fact.c_str(), inst.c_str(), err->error);
                }
            } else {
                errlogPrintf("%s.%s " ERL_WARNING " query yields %u\n",
                             fact.c_str(), inst.c_str(), rep->nlmsg_type);
            }
        });
        if(linStatDebug>=5)
            errlogPrintf("%s.%s INFO submit\n",
                         fact.c_str(), inst.c_str());
    }
};


} // namespace

DEFINE_TABLE(tblName, IFStatTable)
