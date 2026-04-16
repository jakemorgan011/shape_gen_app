#pragma once
#include <string>

class AnalysisEngine;

class OSCSender {
public:
    OSCSender(const std::string& host = "127.0.0.1", int port = 9000);
    void send(AnalysisEngine& engine);

private:
    std::string m_host;
    int         m_port;
};
