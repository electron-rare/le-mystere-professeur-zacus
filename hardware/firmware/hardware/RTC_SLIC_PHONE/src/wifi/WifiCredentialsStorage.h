#ifndef WIFI_CREDENTIALS_STORAGE_H
#define WIFI_CREDENTIALS_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>

class WifiCredentialsStorage {
public:
    static bool load(String& ssid, String& password) {
        Preferences prefs;
        if (!prefs.begin("wifi-creds", false)) return false;
        ssid = prefs.isKey("ssid") ? prefs.getString("ssid", "") : String("");
        password = prefs.isKey("password") ? prefs.getString("password", "") : String("");
        prefs.end();
        return !ssid.isEmpty();
    }

    static void save(const String& ssid, const String& password) {
        Preferences prefs;
        if (!prefs.begin("wifi-creds", false)) return;
        prefs.putString("ssid", ssid);
        prefs.putString("password", password);
        prefs.end();
    }
};

#endif // WIFI_CREDENTIALS_STORAGE_H
