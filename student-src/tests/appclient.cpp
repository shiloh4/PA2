// *****************************************************************************
// This is the client file which you can use to test the Vegas Congestion
// Control algorithm you coded up in src/cc.h
//
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <netdb.h>
#include <numeric>
#include <thread>
#include <udt.h>
#include <unistd.h>

#include "cc.h"
#include "test_util.h"

void *monitor(void *);

int main(int argc, char *argv[]) {
    using namespace std;

    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <server_ip> <server_port>" << endl;
        return 0;
    }

    // Automatically start up and clean up UDT module.
    UDTUpDown _udtContext;

    struct addrinfo hints, *local, *peer;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (0 != getaddrinfo(NULL, "9000", &hints, &local)) {
        cout << "incorrect network address.\n" << endl;
        return 0;
    }

    UDTSOCKET client =
        UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);
    UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<Vegas>,
                    sizeof(CCCFactory<Vegas>));

    freeaddrinfo(local);

    if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer)) {
        cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2]
             << endl;
        return 0;
    }

    // connect to the server, implict bind
    if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen)) {
        cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    freeaddrinfo(peer);

    int singleSendSize = 100000;
    int totalBytesSend = 0;
    char *data = new char[singleSendSize];
    thread monitor_th = thread(monitor, &client);

    while (true) {
        UDT::TRACEINFO perf;
        if (UDT::ERROR == UDT::perfmon(client, &perf)) {
            break;
        }

        if (perf.pktFlightSize < 500) {
            int ssize = 0;
            int ss;

            while (ssize < singleSendSize) {
                if (UDT::ERROR == (ss = UDT::send(client, data + ssize,
                                                  singleSendSize - ssize, 0))) {
                    cout << "send:" << UDT::getlasterror().getErrorMessage()
                         << endl;
                    break;
                }

                ssize += ss;
            }

            if (ssize < singleSendSize)
                break;

            totalBytesSend += ssize;
        } else {
            usleep(1000);
        }
    }

    monitor_th.join();
    UDT::close(client);

    delete[] data;
    return 0;
}

void *monitor(void *s) {
    using namespace std;
    using namespace std::chrono;

    UDTSOCKET u = *(UDTSOCKET *)s;
    UDT::TRACEINFO perf;

    vector<double> sendRates;
    vector<double> RTTs;
    vector<double> CWnds;

    auto start = steady_clock::now();
    int samplesTaken = 0;
    constexpr int samplesPerSecond = 2;
    constexpr size_t samplesToKeep = 90;
    constexpr double numSecondsToRun = 60.0;

    cout << "┌─────────────────┬────────────┬────────┬─────────────┐" << endl
         << "│ SendRate (Mb/s) │  RTT (ms)  │  CWnd  │ PktInFlight │" << endl
         << "├─────────────────┼────────────┼────────┼─────────────┤" << endl;

    while (true) {
        // Sleep for 500ms
        this_thread::sleep_for(milliseconds(1000 / samplesPerSecond));

        if (UDT::ERROR == UDT::perfmon(u, &perf)) {
            cout << "perfmon: " << UDT::getlasterror().getErrorMessage()
                 << endl;
            break;
        }

        if ((samplesTaken % 2) == 0) {
            cout << "│ " << setprecision(3) << fixed << setfill(' ') << setw(15)
                 << perf.mbpsSendRate << " │ " << setfill(' ') << setw(10)
                 << perf.msRTT << " │ " << setfill(' ') << setw(6)
                 << perf.pktCongestionWindow << " │ " << setfill(' ')
                 << setw(11) << perf.pktFlightSize << " │" << endl;
        }

        // Record current sample
        sendRates.push_back(perf.mbpsSendRate);
        RTTs.push_back(perf.msRTT);
        CWnds.push_back(perf.pktCongestionWindow);
        samplesTaken += 1;

        // Keep track of only the last few samples
        if (sendRates.size() > samplesToKeep) {
            sendRates.erase(sendRates.begin());
            RTTs.erase(RTTs.begin());
            CWnds.erase(CWnds.begin());
        }

        auto now = chrono::steady_clock::now();

        if (chrono::duration_cast<chrono::seconds>(now - start).count() >
                numSecondsToRun ||
            perf.pktFlightSize == 0) {
            break;
        }
    }

    // Print table footer
    cout << "└─────────────────┴────────────┴────────┴─────────────┘" << endl
         << endl;

    // Calculate statistics based on reported samples
    double mean = accumulate(sendRates.begin(), sendRates.end(), 0.0);
    mean = mean / sendRates.size();

    double meanCWnd = accumulate(CWnds.begin(), CWnds.end(), 0.0);
    meanCWnd = meanCWnd / CWnds.size();

    double meanRTT = accumulate(RTTs.begin(), RTTs.end(), 0.0);
    meanRTT = meanRTT / RTTs.size();
    double accum;
    for_each(RTTs.begin(), RTTs.end(),
             [&](const double d) { accum += (d - meanRTT) * (d - meanRTT); });

    double stddev = sqrt(accum / RTTs.size());

    cout << "Summary of the last " << (samplesToKeep / samplesPerSecond)
         << " seconds:" << endl;
    cout << "* Average Rate: " << mean << endl;
    cout << "* Average CWnd: " << meanCWnd << endl;
    cout << "* RTT Std Dev:  " << stddev << endl;

    // Regardless of whether the client finished sending or not, we forcefully
    // shuts down the client after a minute.
    exit(0);

    // Not going to be executed, but fine.
    return 0;
}
