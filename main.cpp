#include <iostream>
#include "HttpProxyServer.h"

using namespace std;

int main() {
    cout << "Hello, World!" << endl;

    HttpProxyServer server;
    server.run("0.0.0.0", 4869);

//    cout << strstr("asdf", "sd") << endl;


    return 0;
}