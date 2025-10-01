#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <WebServer.h>
#include "ConfigManager.h"

// forward declarations to avoid circular includes
class SMSManager;
class CallManager;

class WebDashboard {
public:
  // use a literal default port here to avoid include-order macro issues
  WebDashboard(ConfigManager &cfgMgr, SMSManager *sms = nullptr, CallManager *call = nullptr, int port = 420);
  void begin();
  void handleClient();

private:
  WebServer server;
  ConfigManager &cfgMgr;
  SMSManager *smsManager = nullptr;
  CallManager *callManager = nullptr;
  void handleDashboard();
  void handleApiConfig();
  void handleApiEvents();
  void handleSendSms();
  void handleCall();
  void handleHangup();
  bool checkAuth();
};

#endif
