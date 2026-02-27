// AgentSupervisor.cpp
#include "core/AgentSupervisor.h"
#include <Arduino.h>

AgentSupervisor& AgentSupervisor::instance() {
    static AgentSupervisor inst;
    return inst;
}

void AgentSupervisor::notify(const std::string& agent, const AgentStatus& status) {
    statusMap_[agent] = status;
    publishEvent("status_update", agent, status);
}

AgentStatus AgentSupervisor::getStatus(const std::string& agent) const {
    auto it = statusMap_.find(agent);
    if (it != statusMap_.end()) return it->second;
    return {"unknown", "", 0};
}

std::map<std::string, AgentStatus> AgentSupervisor::getAllStatus() const {
    return statusMap_;
}

void AgentSupervisor::subscribe(const std::string& event, std::function<void(const std::string&, const AgentStatus&)> cb) {
    subscribers_[event].push_back(cb);
}

void AgentSupervisor::publishEvent(const std::string& event, const std::string& agent, const AgentStatus& status) {
    for (auto& cb : subscribers_[event]) {
        cb(agent, status);
    }
}
