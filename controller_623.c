#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define UDP_PORT 9500
#define AUTH_TOKEN "OPS-0623"

int main() {

    int sock;

    char buffer[1024];

    struct sockaddr_in server_addr;

    sock =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (sock < 0) {

        perror("Socket creation failed");
        return 1;
    }

    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(PORT);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &server_addr.sin_addr
    );

    if (connect(
            sock,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0) {

        perror("Connection failed");
        close(sock);
        return 1;
    }

    printf(
        "Connected to Agent on port %d.\n",
        PORT
    );

    /*
     * AUTH
     */

    char auth_command[100];

    snprintf(
        auth_command,
        sizeof(auth_command),
        "AUTH %s\n",
        AUTH_TOKEN
    );

    send(
        sock,
        auth_command,
        strlen(auth_command),
        0
    );

    memset(
        buffer,
        0,
        sizeof(buffer)
    );

    int n =
        recv(
            sock,
            buffer,
            sizeof(buffer) - 1,
            0
        );

    if (n > 0) {

        buffer[n] = '\0';

        printf(
            "AUTH Response: %s",
            buffer
        );
    }

    /*
     * SYSINFO
     */

    char *sysinfo_command =
        "SYSINFO\n";

    send(
        sock,
        sysinfo_command,
        strlen(sysinfo_command),
        0
    );

    printf(
        "Sent: SYSINFO\n"
    );

    memset(
        buffer,
        0,
        sizeof(buffer)
    );

    n =
        recv(
            sock,
            buffer,
            sizeof(buffer) - 1,
            0
        );

    if (n > 0) {

        buffer[n] = '\0';

        printf(
            "SYSINFO Response: %s",
            buffer
        );
    }

   /*
 * LISTPROC
 */

char *listproc_command =
    "LISTPROC\n";

send(
    sock,
    listproc_command,
    strlen(listproc_command),
    0
);

printf(
    "Sent: LISTPROC\n"
);

memset(
    buffer,
    0,
    sizeof(buffer)
);

n =
    recv(
        sock,
        buffer,
        sizeof(buffer) - 1,
        0
    );

if (n > 0) {

    buffer[n] = '\0';

    printf(
        "LISTPROC Response: %s",
        buffer
    );
}  
   /*
 * EXEC DATE
 */

char *exec_command =
    "EXEC DATE\n";

send(
    sock,
    exec_command,
    strlen(exec_command),
    0
);

printf(
    "Sent: EXEC DATE\n"
);

memset(
    buffer,
    0,
    sizeof(buffer)
);

n = recv(
    sock,
    buffer,
    sizeof(buffer) - 1,
    0
);

if (n > 0) {

    buffer[n] = '\0';

    printf(
        "EXEC Response: %s",
        buffer
    );
} 
  /*
 * Invalid EXEC test
 */

char *bad_exec_command =
    "EXEC LS\n";

send(
    sock,
    bad_exec_command,
    strlen(bad_exec_command),
    0
);

printf("Sent: EXEC LS\n");

memset(
    buffer,
    0,
    sizeof(buffer)
);

n = recv(
    sock,
    buffer,
    sizeof(buffer) - 1,
    0
);

if (n > 0) {

    buffer[n] = '\0';

    printf(
        "Invalid EXEC Response: %s",
        buffer
    );
}   
   /*
 * PUT test.txt
 */

FILE *fp = fopen("test.txt", "rb");

if (fp == NULL) {

    perror("Cannot open test.txt");
    close(sock);
    return 1;
}

fseek(
    fp,
    0,
    SEEK_END
);

long filesize =
    ftell(fp);

rewind(fp);

char put_command[300];

snprintf(
    put_command,
    sizeof(put_command),
    "PUT test.txt %ld\n",
    filesize
);

send(
    sock,
    put_command,
    strlen(put_command),
    0
);

printf(
    "Sent: PUT test.txt %ld\n",
    filesize
);

char file_buffer[1024];

size_t bytes_read;

while ((bytes_read =
        fread(
            file_buffer,
            1,
            sizeof(file_buffer),
            fp)) > 0) {

    send(
        sock,
        file_buffer,
        bytes_read,
        0
    );
}

fclose(fp);

memset(
    buffer,
    0,
    sizeof(buffer)
);

n = recv(
    sock,
    buffer,
    sizeof(buffer) - 1,
    0
);

if (n > 0) {

    buffer[n] = '\0';

    printf(
        "PUT Response: %s",
        buffer
    );
}
   
   /*
 * GET test.txt
 */

char *get_command =
    "GET test.txt\n";

send(
    sock,
    get_command,
    strlen(get_command),
    0
);

printf("Sent: GET test.txt\n");

memset(
    buffer,
    0,
    sizeof(buffer)
);

n = recv(
    sock,
    buffer,
    sizeof(buffer) - 1,
    0
);

if (n > 0) {

    buffer[n] = '\0';

    printf(
        "GET Response: %s",
        buffer
    );

    char filename[256];
    long filesize;

    if (sscanf(
            buffer,
            "OK FILE_SEND %255s %ld",
            filename,
            &filesize) == 2) {

        FILE *fp =
            fopen(
                "downloaded_test.txt",
                "wb"
            );

        if (fp == NULL) {

            perror(
                "Cannot create downloaded file"
            );

            close(sock);
            return 1;
        }

        long remaining =
            filesize;

        char file_buffer[1024];

        while (remaining > 0) {

            int to_receive =
                remaining > sizeof(file_buffer)
                ? sizeof(file_buffer)
                : remaining;

            int received =
                recv(
                    sock,
                    file_buffer,
                    to_receive,
                    0
                );

            if (received <= 0) {
                break;
            }

            fwrite(
                file_buffer,
                1,
                received,
                fp
            );

            remaining -=
                received;
        }

        fclose(fp);

        if (remaining == 0) {

            printf(
                "File downloaded successfully as downloaded_test.txt\n"
            );
        }
    }
}

   /*
 * UDP MONITOR START
 */

int udp_sock;

struct sockaddr_in udp_addr;

udp_sock = socket(
    AF_INET,
    SOCK_DGRAM,
    0
);

if (udp_sock < 0) {

    perror(
        "UDP socket creation failed"
    );

    close(sock);

    return 1;
}

memset(
    &udp_addr,
    0,
    sizeof(udp_addr)
);

udp_addr.sin_family =
    AF_INET;

udp_addr.sin_addr.s_addr =
    INADDR_ANY;

udp_addr.sin_port =
    htons(UDP_PORT);

if (bind(
        udp_sock,
        (struct sockaddr *)&udp_addr,
        sizeof(udp_addr)) < 0) {

    perror("UDP bind failed");

    close(udp_sock);
    close(sock);

    return 1;
}

char monitor_command[100];

snprintf(
    monitor_command,
    sizeof(monitor_command),
    "MONITOR START %d\n",
    UDP_PORT
);

send(
    sock,
    monitor_command,
    strlen(monitor_command),
    0
);

printf(
    "Sent: MONITOR START %d\n",
    UDP_PORT
);

memset(
    buffer,
    0,
    sizeof(buffer)
);

n = recv(
    sock,
    buffer,
    sizeof(buffer) - 1,
    0
);

if (n > 0) {

    buffer[n] = '\0';

    printf(
        "Monitor Response: %s",
        buffer
    );
}


/*
 * Receive 3 UDP monitoring messages
 */

printf(
    "Waiting for UDP monitoring data...\n"
);

for (int i = 0; i < 3; i++) {

    char udp_buffer[1024];

    memset(
        udp_buffer,
        0,
        sizeof(udp_buffer)
    );

    int udp_received =
        recvfrom(
            udp_sock,
            udp_buffer,
            sizeof(udp_buffer) - 1,
            0,
            NULL,
            NULL
        );

    if (udp_received > 0) {

        udp_buffer[udp_received] =
            '\0';

        printf(
            "UDP Monitor: %s\n",
            udp_buffer
        );
    }
}


/*
 * MONITOR STOP
 */

char *stop_command =
    "MONITOR STOP\n";

send(
    sock,
    stop_command,
    strlen(stop_command),
    0
);

printf(
    "Sent: MONITOR STOP\n"
);

memset(
    buffer,
    0,
    sizeof(buffer)
);

n = recv(
    sock,
    buffer,
    sizeof(buffer) - 1,
    0
);

if (n > 0) {

    buffer[n] = '\0';

    printf(
        "Monitor Response: %s",
        buffer
    );
}

   /*
 * QUIT
 */

char *quit_command =
    "QUIT\n";

send(
    sock,
    quit_command,
    strlen(quit_command),
    0
);

printf(
    "Sent: QUIT\n"
);

memset(
    buffer,
    0,
    sizeof(buffer)
);

n = recv(
    sock,
    buffer,
    sizeof(buffer) - 1,
    0
);

if (n > 0) {

    buffer[n] = '\0';

    printf(
        "QUIT Response: %s",
        buffer
    );
}

close(udp_sock);

    close(sock);

    return 0;
}
