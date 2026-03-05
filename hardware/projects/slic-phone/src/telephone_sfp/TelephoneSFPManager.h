#ifndef TELEPHONE_SFP_MANAGER_H
#define TELEPHONE_SFP_MANAGER_H

#include "telephony/TelephonyService.h"

class TelephoneSFPManager {
public:
    TelephoneSFPManager();
    void attachService(TelephonyService* service);
    void begin();
    void triggerIncomingCall();
    void monitorState();
    TelephonyState state() const;

private:
    TelephonyService* service_;
};

#endif  // TELEPHONE_SFP_MANAGER_H
