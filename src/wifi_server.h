#pragma once

// ---------------------------------------------------------------------------
// wifi_server.h
// Manages the WiFi connection (AP or Station) and an HTTP server that serves
// a mobile-optimised settings page at http://<device-ip>/
//
// Usage:
//   In setup()  → wifiServer.begin();
//   In loop()   → wifiServer.handle();
// ---------------------------------------------------------------------------

class WifiServer {
public:
    // Bring up WiFi (AP or STA based on g_settings) and start the HTTP server.
    void begin();

    // Process any pending HTTP requests — call every iteration of loop().
    void handle();
};

extern WifiServer wifiServer;