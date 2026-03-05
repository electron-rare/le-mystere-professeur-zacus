#include "core/AgentSupervisor.h"
#include <Arduino.h>

void notifySLIC(const std::string& state, const std::string& error = "") {
    AgentStatus status{state, error, millis()};
    AgentSupervisor::instance().notify("slic", status);
}
#include "slic/SLICManager.h"

SLICManager::SLICManager(SlicController* controller)
    : controller_(controller), state_(SLICLineState::UNINITIALIZED), incoming_ring_(false) {}

void SLICManager::attachController(SlicController* controller) {
    controller_ = controller;
}

void SLICManager::begin() {
    if (controller_ == nullptr) {
        state_ = SLICLineState::UNINITIALIZED;
        notifySLIC("uninitialized", "no controller");
        return;
    }
    state_ = controller_->isHookOff() ? SLICLineState::OFF_HOOK : SLICLineState::ON_HOOK;
    notifySLIC(state_ == SLICLineState::OFF_HOOK ? "off_hook" : "on_hook");
}

bool SLICManager::begin(const SlicPins& pins) {
    if (controller_ == nullptr || !controller_->begin(pins)) {
        state_ = SLICLineState::UNINITIALIZED;
        notifySLIC("uninitialized", "begin failed");
        return false;
    }
    begin();
    return true;
}

void SLICManager::monitorLine() {
    if (controller_ == nullptr) {
        state_ = SLICLineState::UNINITIALIZED;
        notifySLIC("uninitialized", "no controller");
        return;
    }
    controller_->tick();
    if (incoming_ring_) {
        state_ = SLICLineState::RINGING;
        notifySLIC("ringing");
    } else {
        state_ = controller_->isHookOff() ? SLICLineState::OFF_HOOK : SLICLineState::ON_HOOK;
        notifySLIC(state_ == SLICLineState::OFF_HOOK ? "off_hook" : "on_hook");
    }
}

void SLICManager::controlCall() {
    controlCall(incoming_ring_);
}

void SLICManager::controlCall(bool incoming_ring) {
    incoming_ring_ = incoming_ring;
    if (controller_ == nullptr) {
        notifySLIC("uninitialized", "no controller");
        return;
    }
    if (incoming_ring_) {
        controller_->setRing(true);
        state_ = SLICLineState::RINGING;
        notifySLIC("ringing");
    } else {
        controller_->setRing(false);
        state_ = controller_->isHookOff() ? SLICLineState::OFF_HOOK : SLICLineState::ON_HOOK;
        notifySLIC(state_ == SLICLineState::OFF_HOOK ? "off_hook" : "on_hook");
    }
}

SLICLineState SLICManager::state() const {
    return state_;
}

bool SLICManager::isHookOff() const {
    return controller_ != nullptr && controller_->isHookOff();
}
