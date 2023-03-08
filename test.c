#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>

int main()
{
    int sock = nn_socket(AF_SP, NN_PUB);
    nn_bind(sock, "TCP://127.0.0.1:5555");
    const char* msg = "Hello, world!";
    nn_send(sock, msg, strlen(msg) + 1, 0);
    nn_shutdown(sock, 0);
    return 0;
}





