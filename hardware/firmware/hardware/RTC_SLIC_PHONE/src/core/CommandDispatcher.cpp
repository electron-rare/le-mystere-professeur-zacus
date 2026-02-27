#include "core/CommandDispatcher.h"

#include <algorithm>

void CommandDispatcher::registerCommand(const String& name, Handler handler) {
    const String key = normalizeCommand(name);
    if (key.isEmpty() || !handler) {
        return;
    }

    if (handlers_.find(key) == handlers_.end()) {
        order_.push_back(key);
    }
    handlers_[key] = std::move(handler);
}

DispatchResponse CommandDispatcher::dispatch(const String& line) const {
    String input = line;
    input.trim();
    if (input.isEmpty()) {
        DispatchResponse resp;
        resp.ok = false;
        resp.code = "EMPTY_COMMAND";
        return resp;
    }

    const int sep = input.indexOf(' ');
    const String cmd = normalizeCommand(sep > 0 ? input.substring(0, sep) : input);
    const String args = sep > 0 ? input.substring(sep + 1) : "";

    const auto it = handlers_.find(cmd);
    if (it == handlers_.end()) {
        DispatchResponse resp;
        resp.ok = false;
        resp.code = "unsupported_command";
        if (!cmd.isEmpty()) {
            resp.code += ' ';
            resp.code += cmd;
        }
        return resp;
    }

    return it->second(args);
}

bool CommandDispatcher::hasCommand(const String& name) const {
    return handlers_.find(normalizeCommand(name)) != handlers_.end();
}

String CommandDispatcher::helpText() const {
    String out;
    out.reserve(order_.size() * 24);
    for (size_t i = 0; i < order_.size(); ++i) {
        out += order_[i];
        if (i + 1 < order_.size()) {
            out += '\n';
        }
    }
    return out;
}

std::vector<String> CommandDispatcher::commands() const {
    return order_;
}

String CommandDispatcher::normalizeCommand(const String& name) {
    String out = name;
    out.trim();
    out.toUpperCase();
    return out;
}
