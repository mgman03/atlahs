#ifndef ATLAHS_MESSAGE_MATCHING_HPP
#define ATLAHS_MESSAGE_MATCHING_HPP

#include "LogGOPSim.hpp"
#include <list>
#include <vector>

// Shared by the standalone scheduler and the htsim backend.
struct ruqelem_t {
    btime_t starttime = 0;
    std::uint32_t size = 0, src = 0, tag = 0, offset = 0, stream = 0, gpu = 0, peer_gpu = 0;
    MessageIdentity message_id;
};
using ruq_t = std::list<ruqelem_t>;

static inline int match(const graph_node_properties& elem, ruq_t* queue,
                        ruqelem_t* result = nullptr) {
    int attempts = 0;
    auto selected = queue->end();
    for (auto iter = queue->begin(); iter != queue->end(); ++iter) {
        ++attempts;
        if (elem.target != ANY_SOURCE && iter->src != ANY_SOURCE && iter->src != elem.target)
            continue;
        if (!same_message(elem.message_id, elem.tag, iter->message_id, iter->tag)) continue;
        if (elem.message_id.explicit_fields &&
            (elem.gpu != iter->gpu || elem.peer_gpu != iter->peer_gpu)) continue;
        if (selected == queue->end() || iter->offset < selected->offset) selected = iter;
    }
    if (selected == queue->end()) return -1;
    if (result) *result = *selected;
    queue->erase(selected);
    return attempts;
}

typedef unsigned int uint;
typedef unsigned long int ulint;
typedef unsigned long long int ullint;

#endif
