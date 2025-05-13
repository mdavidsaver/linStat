/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* "ethtool -S <name>" low level / NIC driver statistics.
 *
 * This information is exposed both by an ioctl() and
 * a "generic" netlink interface.  As of ethtool 6.1
 * the ioctl() method is used, so we do likewise.
 */

#include <string>
#include <vector>
#include <map>

#include <osiSock.h>

#include <string.h>
#include <errno.h>

#include <linux/sockios.h>
#include <linux/ethtool.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_link.h>

#include <errlog.h>
#include <epicsThread.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "ethtool";

enum AggStat {
    AggNone,
    AggRXDrop,
    AggTXDrop,
};

struct Socket {
    const SOCKET q;
    Socket()
        :q(epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_IP))
    {
        if(q==INVALID_SOCKET)
            throw std::runtime_error("Unable to allocate query socket");
    }
    ~Socket() {
        epicsSocketDestroy(q);
    }
};

struct EthtoolTable : public StatTable {
    const Socket sock;

    // const after ctor
    struct ethtool_drvinfo info{};
    std::vector<std::pair<std::string, AggStat>> stat_names;

    explicit EthtoolTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {
        if(inst.empty())
            throw std::runtime_error("Empty instance not allowed");
        else if(inst.size()>=IFNAMSIZ)
            throw std::runtime_error("interface name too long");

        bool supported = false;
        info.cmd = ETHTOOL_GDRVINFO;
        if(send_ioctl(&info)) {
            auto err = errno;
            if(err!=ENOTSUP)
                errlogPrintf(ERL_ERROR ": Unable to query drvinfo err=%d\n", err);

            strcpy(info.driver, "N/A");
            strcpy(info.version, "N/A");

        } else {
            supported = true;
        }


        if(supported && info.n_stats) {
            std::vector<std::string> names;
            get_strings(ETH_SS_STATS, names);

            stat_names.reserve(names.size());
            for(auto& name : names) {
                AggStat agg = AggNone;
                /* These driver specific stat name strings are anything but consistent.  eg.
                 * "rx_drops", "[0] rx_discards", "rx_queue_0_drops", ...
                 * all have (probably...) the same meaning.  Some packets dropped
                 * into the bit bucket.
                 * The common denominators seen so far are "drop" and "discard"
                 * So do some sub-string matching in an attempt to classify.
                 */
                if(name.find("drop")!=name.npos || name.find("discard")!=name.npos) {
                    if(name.find("rx")!=name.npos) {
                        agg = AggRXDrop;

                    } else if(name.find("tx")!=name.npos) {
                        agg = AggTXDrop;
                    }
                }
                stat_names.emplace_back(std::make_pair(std::move(name), agg));
            }
        }
    }
    virtual ~EthtoolTable() {}

    virtual void update() override final {
        Transaction tr(*this);
        tr.set("driver", info.driver);
        tr.set("version", info.version);

        if(stat_names.empty()) {
            tr.set("found_stat", 0);
            tr.set("rx_dropped", 0);
            tr.set("tx_dropped", 0);
            return;
        }

        std::vector<char> buf(sizeof(ethtool_stats) + stat_names.size()*sizeof(uint64_t));
        auto stats = reinterpret_cast<ethtool_stats*>(buf.data());
        stats->cmd = ETHTOOL_GSTATS;
        stats->n_stats = stat_names.size();

        if(!send_ioctl(stats)) {
            const auto out = reinterpret_cast<uint64_t*>(&buf[sizeof(ethtool_stats)]);

            uint64_t rx_drop = 0u, tx_drop = 0u;
            bool found_stat = false;

            for(size_t i=0; i<stat_names.size(); i++) {
                tr.set(SB()<<"stat."<<stat_names[i].first, out[i]);

                switch(stat_names[i].second) {
                case AggNone:
                    break;
                case AggRXDrop:
                    rx_drop += out[i];
                    found_stat = true;
                    break;
                case AggTXDrop:
                    tx_drop += out[i];
                    found_stat = true;
                    break;
                }
            }

            tr.set("rx_dropped", rx_drop);
            tr.set("tx_dropped", tx_drop);
            tr.set("found_stat", found_stat);
        }
    }

    int send_ioctl(void* data) const {
        struct ifreq req = {};
        inst.copy(req.ifr_name, IFNAMSIZ-1);
        req.ifr_name[IFNAMSIZ-1] = '\0';
        req.ifr_data = (char*)data;

        return ioctl(sock.q, SIOCETHTOOL, &req);
    }

    void get_strings(enum ethtool_stringset set_id,
                     std::vector<std::string>& strs) const
    {
        uint32_t count;
        std::vector<char> buf;

        {
            // one dword used as output for each bit in sset_mask.
            // we only set one bit at a time.
            buf.resize(sizeof(ethtool_sset_info) + sizeof(uint32_t));
            auto sset_info = reinterpret_cast<ethtool_sset_info*>(buf.data());

            // Query number of strings in table.
            sset_info->cmd = ETHTOOL_GSSET_INFO;
            sset_info->sset_mask = 1ULL << set_id;
            if (send_ioctl(sset_info)) {
                auto err = errno;
                if(err!=ENOTSUP)
                    throw std::runtime_error(SB()<<"Unable to query string set info "<<set_id<<" err="<<err);
                // TODO: use ETHTOOL_GDRVINFO fallback for "old kernel" versions (pre v3.7 ?).
                //       take count from out of band.  eg. drvinfo::n_stat .
                return;

            } else if(sset_info->sset_mask==0) {
                return;
            }
            count = sset_info->data[0];
        }

        {
            buf.resize(sizeof(ethtool_gstrings) + count*ETH_GSTRING_LEN);
            auto strings = reinterpret_cast<ethtool_gstrings*>(buf.data());
            strings->cmd = ETHTOOL_GSTRINGS;
            strings->string_set = set_id;
            strings->len = count;

            // read out strings
            if (send_ioctl(strings)) {
                auto err = errno;
                throw std::runtime_error(SB()<<"Unable to query string set "<<set_id<<" err="<<err);
            }

            strs.resize(count);

            for(uint32_t i=0u; i<count; i++) {
                const auto name = &buf[sizeof(ethtool_gstrings) + i*ETH_GSTRING_LEN];
                auto& out = strs[i];

                out.assign(name, strnlen(name, ETH_GSTRING_LEN));
            }
        }
    }
};


} // namespace

DEFINE_TABLE(tblName, EthtoolTable)
