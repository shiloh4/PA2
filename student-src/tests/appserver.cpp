// This is the server file which you can use to test the Vegas Congestion
// Control algorithm you coded up in app/cc.h Under "UDT Options" section,
// uncomment the line corresponding to option UDT_CC, CHANGE class name from
// CUDPBlast to Vegas.

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <udt.h>
#include <unistd.h>

#include "cc.h"
#include "test_util.h"

void *recvdata(void *);

int main(int argc, char *argv[]) {
    using namespace std;

    if ((argc < 2) || (0 == atoi(argv[1]))) {
        cout << "Usage: " << argv[0] << " <server_port>" << endl;
        return 0;
    }

    // Automatically start up and clean up UDT module.
    UDTUpDown _udtContext;

    addrinfo hints;
    addrinfo *res;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    string service("9000");
    if (2 == argc)
        service = argv[1];

    if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res)) {
        cout << "illegal port number or port is busy.\n" << endl;
        return 0;
    }

    UDTSOCKET serv =
        UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<Vegas>,
                    sizeof(CCCFactory<Vegas>));

    if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen)) {
        cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    freeaddrinfo(res);

    cout << "server is ready at port: " << service << endl;

    if (UDT::ERROR == UDT::listen(serv, 10)) {
        cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    sockaddr_storage clientaddr;
    int addrlen = sizeof(clientaddr);

    UDTSOCKET recver;

    while (true) {
        if (UDT::INVALID_SOCK ==
            (recver = UDT::accept(serv, (sockaddr *)&clientaddr, &addrlen))) {
            cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
            return 0;
        }

        char clienthost[NI_MAXHOST];
        char clientservice[NI_MAXSERV];
        getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost,
                    sizeof(clienthost), clientservice, sizeof(clientservice),
                    NI_NUMERICHOST | NI_NUMERICSERV);
        cout << "new connection: " << clienthost << ":" << clientservice
             << endl;

        pthread_t rcvthread;
        pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(recver));
        pthread_detach(rcvthread);
    }

    UDT::close(serv);

    return 0;
}

void *recvdata(void *usocket) {
    using namespace std;

    UDTSOCKET recver = *(UDTSOCKET *)usocket;
    delete (UDTSOCKET *)usocket;

    char *data;
    int size = 100000;
    data = new char[size];

    while (true) {
        int rsize = 0;
        int rs;
        while (rsize < size) {
            int rcv_size;
            int var_size = sizeof(int);
            UDT::getsockopt(recver, 0, UDT_RCVDATA, &rcv_size, &var_size);
            if (UDT::ERROR ==
                (rs = UDT::recv(recver, data + rsize, size - rsize, 0))) {
                cout << "recv:" << UDT::getlasterror().getErrorMessage()
                     << endl;
                break;
            }

            rsize += rs;
        }

        if (rsize < size)
            break;
    }

    delete[] data;

    UDT::close(recver);

    return NULL;
}
