#include "OSCSender.h"
#include "geo/analysis.h"
#include "tinyosc.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

OSCSender::OSCSender(const std::string& host, int port)
    : m_host(host), m_port(port) {}

void OSCSender::send(AnalysisEngine& engine) {
    if (!engine.ready()) return;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)m_port);
    inet_pton(AF_INET, m_host.c_str(), &addr.sin_addr);

    char buf[256];

    for (const auto& [key, val] : engine.get_stats()) {
        std::string address = "/shapegen/" + key;
        uint32_t len = tosc_writeMessage(buf, sizeof(buf), address.c_str(), "f", val);
        sendto(sock, buf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    }

    for (int i = 0; i < (int)engine.get_y_profile().size(); i++) {
        char address[64];
        snprintf(address, sizeof(address), "/shapegen/y_profile/%d", i);
        uint32_t len = tosc_writeMessage(buf, sizeof(buf), address, "f", engine.get_y_profile()[i]);
        sendto(sock, buf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    }

    for (int i = 0; i < (int)engine.get_z_profile().size(); i++) {
        char address[64];
        snprintf(address, sizeof(address), "/shapegen/z_profile/%d", i);
        uint32_t len = tosc_writeMessage(buf, sizeof(buf), address, "f", engine.get_z_profile()[i]);
        sendto(sock, buf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    }

    close(sock);
}
