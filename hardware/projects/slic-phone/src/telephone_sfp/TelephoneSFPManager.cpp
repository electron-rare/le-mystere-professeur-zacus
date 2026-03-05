#include "telephone_sfp/TelephoneSFPManager.h"

TelephoneSFPManager::TelephoneSFPManager() : service_(nullptr) {}

void TelephoneSFPManager::attachService(TelephonyService* service) {
    service_ = service;
}

void TelephoneSFPManager::begin() {}

void TelephoneSFPManager::triggerIncomingCall() {
    if (service_ == nullptr) {
        return;
    }
    service_->triggerIncomingRing();
}

void TelephoneSFPManager::monitorState() {
    if (service_ == nullptr) {
        return;
    }
    service_->tick();
}

TelephonyState TelephoneSFPManager::state() const {
    if (service_ == nullptr) {
        return TelephonyState::IDLE;
    }
    return service_->state();
}
