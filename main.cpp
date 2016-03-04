#include <iostream>
#include "HttpProxyServer.h"
#include "datapipe/DefaultDataPipe.h"

void test();

using namespace std;

int main() {
    cout << "Hello Http Proxy Server!" << endl;

    HttpProxyServer server;
    server.run("0.0.0.0", 4869);
//    test();

    return 0;
}

void test() {
//    DataPipe sp(STDIN_FILENO);
//    sp.pipe(STDOUT_FILENO, "Hello World", 11);

    char address[100], port[100] = "80";
//    int port;
    int n;

    if ((n = sscanf("CONNECT google.com:443 HTTP/1.1", "CONNECT %255[^:]:%5s HTTP/", address, port)) > 0)
    {
        cout << address << " : " << port << endl;
    }
    else
    {
        cout << "found:" << n << endl;
    }

    if ((n = sscanf("GET http://localhost/ HTTP/1.1", "%*s %*[^:/]://%255[^:/]:%5[^:/] HTTP/", address, port)) > 0)
    {
        cout << address << " : " << port << endl;
    }
    else
    {
        cout << "found:" << n << endl;
    }
}
