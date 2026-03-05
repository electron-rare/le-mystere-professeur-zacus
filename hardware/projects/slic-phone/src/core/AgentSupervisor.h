// AgentSupervisor.h
// Superviseur central pour la coordination des agents RTC_BL_PHONE
#pragma once
#include <map>
#include <string>
#include <functional>
#include <vector>

struct AgentStatus {
    std::string state;
    std::string lastError;
    unsigned long lastUpdate;
};

class AgentSupervisor {
public:
    static AgentSupervisor& instance();
    void notify(const std::string& agent, const AgentStatus& status);
    AgentStatus getStatus(const std::string& agent) const;
    std::map<std::string, AgentStatus> getAllStatus() const;
    void subscribe(const std::string& event, std::function<void(const std::string&, const AgentStatus&)> cb);
    void publishEvent(const std::string& event, const std::string& agent, const AgentStatus& status);
private:
    AgentSupervisor() = default;
    std::map<std::string, AgentStatus> statusMap_;
    std::map<std::string, std::vector<std::function<void(const std::string&, const AgentStatus&)>>> subscribers_;
};
