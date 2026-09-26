#pragma once
class WebServer;

void firmwareUpdateBegin(WebServer& server);
void firmwareUpdatePoll();
void firmwareUpdateAfterHttp();
bool firmwareUpdateBusy();
