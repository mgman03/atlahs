#pragma once

#include "LogGOPSim.hpp"
#include "simulators/atlahs/AtlahsAdapter.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using DeserializedNode = traceschema::AtlahsNode;

// ATLAHS's per-rank ready queue and completion timestamps. The adapter owns all trace access.
class RankSchedule {
public:
    RankSchedule(traceschema::AtlahsAdapter& reader, std::uint32_t rank)
        : reader_(&reader), rank_(rank), ready_nodes_(reader.read_root_nodes(rank)) {}

    std::uint32_t GetNumNodes() const { return reader_->get_node_count(rank_); }

    using nodelist_t = std::vector<graph_node_properties>;
    void GetExecutableNodes(nodelist_t* output) {
        for (const auto& node : ready_nodes_) {
            graph_node_properties event{};
            event.target = node.Peer;
            event.size = node.Size;
            event.tag = node.Tag;
            event.stream = node.Stream;
            event.nic = node.Nic;
            event.gpu = node.Gpu;
            event.peer_gpu = node.PeerGpu;
            if (reader_->uses_explicit_metadata())
                event.message_id = {true, node.Communicator, node.Channel,
                                    node.Sequence, node.MessageKind, node.Chunk};
            event.starttime = node.start_time;
            switch (static_cast<traceschema::OperationType>(node.Type)) {
            case traceschema::OperationType::Send: event.type = OP_SEND; break;
            case traceschema::OperationType::Receive: event.type = OP_RECV; break;
            case traceschema::OperationType::Calculation: event.type = OP_LOCOP; break;
            default: throw std::invalid_argument("Invalid ATLAHS schedule operation type");
            }
            event.offset = node.node_index; // htsim's event API calls this an offset.
            output->push_back(event);
        }
        ready_nodes_.clear();
    }

    void MarkNodeAsStarted(std::uint32_t index) {
        release_dependents(index, traceschema::DependencyEvent::Start);
    }
    void MarkNodeAsDone(std::uint32_t index) {
        release_dependents(index, traceschema::DependencyEvent::Finish);
    }
    void MarkNodeAsDone(std::uint32_t index, std::uint64_t completion_time) {
        release_dependents(index, traceschema::DependencyEvent::Finish, completion_time);
    }

private:
    void release_dependents(std::uint32_t index, traceschema::DependencyEvent event,
                            std::optional<std::uint64_t> completion_time = std::nullopt) {
        for (const auto dependent : reader_->read_dependents(rank_, index, event)) {
            const auto remaining = reader_->decrement_dependency_count(rank_, dependent);
            // Track every completed prerequisite, even while other dependencies remain.
            // A join starts no earlier than the latest of those completion timestamps.
            auto timestamp = completion_times_.end();
            if (completion_time) {
                timestamp = completion_times_.try_emplace(dependent, *completion_time).first;
                timestamp->second = std::max(timestamp->second, *completion_time);
            }
            if (remaining == 0) {
                auto node = reader_->read_node(rank_, dependent);
                if (timestamp != completion_times_.end()) {
                    node.start_time = timestamp->second;
                    completion_times_.erase(timestamp);
                }
                ready_nodes_.push_back(std::move(node));
            }
        }
    }

    traceschema::AtlahsAdapter* reader_;
    std::uint32_t rank_;
    std::vector<DeserializedNode> ready_nodes_;
    std::map<std::uint32_t, std::uint64_t> completion_times_;
};

// htsim's input entry point. It owns an adapter and rank schedules, with no format parsing.
class Parser {
private:
    std::unique_ptr<traceschema::AtlahsAdapter> reader_;
public:
    using schedules_t = std::vector<RankSchedule>;
    // Declared after the adapter so schedules and their borrowed views are destroyed first.
    schedules_t schedules;

    Parser(const std::string& filename, bool save_mem)
        : reader_(std::make_unique<traceschema::AtlahsAdapter>(filename.c_str(), save_mem)) {
        schedules.reserve(reader_->get_rank_count());
        for (std::uint32_t rank = 0; rank < reader_->get_rank_count(); ++rank)
            schedules.emplace_back(*reader_, rank);
        if (save_mem) {
            reader_->invalidate_shared_input();
            std::printf("The schedule will be invalid after this simulation!\n");
        }
    }

    std::uint32_t GetNumStreams() const { return reader_->get_stream_count(); }
    bool UsesExplicitGpuMapping() const { return reader_->uses_explicit_metadata(); }
    std::uint32_t GetNumGPU() const { return reader_->get_gpu_count(); }
};
