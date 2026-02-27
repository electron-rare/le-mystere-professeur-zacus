#ifndef CORE_COMMAND_DISPATCHER_H
#define CORE_COMMAND_DISPATCHER_H

#include <Arduino.h>

#include <functional>
#include <map>
#include <vector>

struct DispatchResponse {
    bool ok = true;
    String code = "";
    String json;
    String raw;
};

class CommandDispatcher {
public:
    using Handler = std::function<DispatchResponse(const String& args)>;

    void registerCommand(const String& name, Handler handler);
    DispatchResponse dispatch(const String& line) const;
    bool hasCommand(const String& name) const;
    String helpText() const;
    std::vector<String> commands() const;

private:
    std::map<String, Handler> handlers_;
    std::vector<String> order_;

    static String normalizeCommand(const String& name);
};

#endif  // CORE_COMMAND_DISPATCHER_H
