/*
 * Example of SSH tunnel using libssh2.
 *
 * This code was based on the 'tcpip-forward.c' example:
 * https://github.com/libssh2/libssh2/blob/master/example/tcpip-forward.c
 *
 * Author: marianafranco (https://github.com/marianafranco)
 */

#include <libssh2.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>

#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t)-1
#endif

const char *keyfile1 = "/home/username/.ssh/id_rsa.pub";
const char *keyfile2 = "/home/username/.ssh/id_rsa";
const char *username = "username";
const char *password = "";

const char *server_ip = "127.0.0.1";

const char *remote_listenhost = "localhost"; /* resolved by the remote server */
int remote_wantport = 4000;
int remote_listenport;

const char *local_destip = "127.0.0.1";
int local_destport = 8080;

enum {
    AUTH_NONE = 0,
    AUTH_PASSWORD,
    AUTH_PUBLICKEY
};


int forward_tunnel(LIBSSH2_SESSION *session, LIBSSH2_CHANNEL *channel) {
    int i, rc = 0;
    struct sockaddr_in sin;
    fd_set fds;
    struct timeval tv;
    ssize_t len, wr;
    char buf[16384];
    int forwardsock = -1;

    fprintf(stdout,
        "Accepted remote connection. Connecting to local server %s:%d\n",
        local_destip, local_destport);
    forwardsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (forwardsock == -1) {
        fprintf(stderr, "Error opening socket\n");
        goto shutdown;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(local_destport);
    if (INADDR_NONE == (sin.sin_addr.s_addr = inet_addr(local_destip))) {
        fprintf(stderr, "Invalid local IP address\n");
        goto shutdown;
    }

    if (-1 == connect(forwardsock, (struct sockaddr *)&sin,
          sizeof(struct sockaddr_in))) {
        fprintf(stderr, "Failed to connect!\n");
        goto shutdown;
    }

    fprintf(stdout, "Forwarding connection from remote %s:%d to local %s:%d\n",
        remote_listenhost, remote_listenport, local_destip, local_destport);

    /* Setting session to non-blocking IO */
    libssh2_session_set_blocking(session, 0);

    while (1) {
        FD_ZERO(&fds);
        FD_SET(forwardsock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        rc = select(forwardsock + 1, &fds, NULL, NULL, &tv);
        if (-1 == rc) {
            fprintf(stderr, "Socket not ready!\n");
            goto shutdown;
        }
        if (rc && FD_ISSET(forwardsock, &fds)) {
            len = recv(forwardsock, buf, sizeof(buf), 0);
            if (len < 0) {
                fprintf(stderr, "Error reading from the forwardsock!\n");
                goto shutdown;
            } else if (0 == len) {
                fprintf(stderr, "The local server at %s:%d disconnected!\n",
                    local_destip, local_destport);
                goto shutdown;
            }
            wr = 0;
            do {
                i = libssh2_channel_write(channel, buf, len);
                if (i < 0) {
                    fprintf(stderr, "Error writing on the SSH channel: %d\n", i);
                    goto shutdown;
                }
                wr += i;
            } while(i > 0 && wr < len);
        }
        while (1) {
            len = libssh2_channel_read(channel, buf, sizeof(buf));
            if (LIBSSH2_ERROR_EAGAIN == len)
                break;
            else if (len < 0) {
                fprintf(stderr, "Error reading from the SSH channel: %d\n", (int)len);
                goto shutdown;
            }
            wr = 0;
            while (wr < len) {
                i = send(forwardsock, buf + wr, len - wr, 0);
                if (i <= 0) {
                    fprintf(stderr, "Error writing on the forwardsock!\n");
                    goto shutdown;
                }
                wr += i;
            }
            if (libssh2_channel_eof(channel)) {
                fprintf(stderr, "The remote client at %s:%d disconnected!\n",
                    remote_listenhost, remote_listenport);
                goto shutdown;
            }
        }
    }

shutdown:
    close(forwardsock);
    /* Setting the session back to blocking IO */
    libssh2_session_set_blocking(session, 1);
    return rc;
}


int main(int argc, char *argv[]) {
    int rc, i, auth = AUTH_NONE;
    struct sockaddr_in sin;
    const char *fingerprint;
    char *userauthlist;
    LIBSSH2_SESSION *session;
    LIBSSH2_LISTENER *listener = NULL;
    LIBSSH2_CHANNEL *channel = NULL;

    int sock = -1;

    if (argc > 1)
        server_ip = argv[1];
    if (argc > 2)
        username = argv[2];
    if (argc > 3)
        password = argv[3];
    if (argc > 4)
        remote_listenhost = argv[4];
    if (argc > 5)
        remote_wantport = atoi(argv[5]);
    if (argc > 6)
        local_destip = argv[6];
    if (argc > 7)
        local_destport = atoi(argv[7]);

    rc = libssh2_init (0);
    if (rc != 0) {
        fprintf (stderr, "libssh2 initialization failed (%d)\n", rc);
        return 1;
    }

    /* Connect to SSH server */
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        fprintf(stderr, "Error opening socket\n");
        return -1;
    }

    sin.sin_family = AF_INET;
    if (INADDR_NONE == (sin.sin_addr.s_addr = inet_addr(server_ip))) {
        fprintf(stderr, "Invalid remote IP address\n");
        return -1;
    }
    sin.sin_port = htons(22); /* SSH port */
    if (connect(sock, (struct sockaddr*)(&sin),
                sizeof(struct sockaddr_in)) != 0) {
        fprintf(stderr, "Failed to connect!\n");
        return -1;
    }

    /* Create a session instance */
    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "Could not initialize the SSH session!\n");
        return -1;
    }

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake(session, sock);
    if(rc) {
        fprintf(stderr, "Error when starting up SSH session: %d\n", rc);
        return -1;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    fprintf(stdout, "Fingerprint: ");
    for(i = 0; i < 20; i++)
        fprintf(stdout, "%02X ", (unsigned char)fingerprint[i]);
    fprintf(stdout, "\n");

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, username, strlen(username));
    fprintf(stderr, "Authentication methods: %s\n", userauthlist);
    if (strstr(userauthlist, "password"))
        auth |= AUTH_PASSWORD;
    if (strstr(userauthlist, "publickey"))
        auth |= AUTH_PUBLICKEY;

    /* check for options */
    if(argc > 8) {
        if ((auth & AUTH_PASSWORD) && !strcasecmp(argv[8], "-p"))
            auth = AUTH_PASSWORD;
        if ((auth & AUTH_PUBLICKEY) && !strcasecmp(argv[8], "-k"))
            auth = AUTH_PUBLICKEY;
    }

    if (auth & AUTH_PASSWORD) {
        if (libssh2_userauth_password(session, username, password)) {
            fprintf(stderr, "Authentication by password failed.\n");
            goto shutdown;
        }
    } else if (auth & AUTH_PUBLICKEY) {
        if (libssh2_userauth_publickey_fromfile(session, username, keyfile1,
                                                keyfile2, password)) {
            fprintf(stderr, "\tAuthentication by public key failed!\n");
            goto shutdown;
        }
        fprintf(stderr, "\tAuthentication by public key succeeded.\n");
    } else {
        fprintf(stderr, "No supported authentication methods found!\n");
        goto shutdown;
    }

    fprintf(stdout, "Asking server to listen on remote %s:%d\n",
        remote_listenhost, remote_wantport);

    listener = libssh2_channel_forward_listen_ex(session, remote_listenhost,
        remote_wantport, &remote_listenport, 1);
    if (!listener) {
        fprintf(stderr, "Could not start the tcpip-forward listener!\n"
                "(Note that this can be a problem at the server!"
                " Please review the server logs.)\n");
        goto shutdown;
    }

    fprintf(stdout, "Server is listening on %s:%d\n", remote_listenhost,
        remote_listenport);

    while (1) {
        fprintf(stdout, "Waiting for remote connection\n");
        channel = libssh2_channel_forward_accept(listener);
        if (!channel) {
            fprintf(stderr, "Could not accept connection!\n"
                    "(Note that this can be a problem at the server!"
                    " Please review the server logs.)\n");
            goto shutdown;
        }

        forward_tunnel(session, channel);

        libssh2_channel_free(channel);
    }

shutdown:
    if (channel)
        libssh2_channel_free(channel);
    if (listener)
        libssh2_channel_forward_cancel(listener);
    libssh2_session_disconnect(session, "Client disconnecting normally");
    libssh2_session_free(session);
    close(sock);
    libssh2_exit();
    return 0;
}
