#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <time.h>

#define PORT 9410
#define SID "3260"
#define AUTH_TOKEN "OPS-0623"
#define LOG_FILE "remoteops_IT24100623.log"
#define STORAGE_DIR "./agentfiles/IT24100623/"

/* Function prototype */
void write_log(const char *message);

/* Monitoring variables */
volatile int monitor_running = 0;
pthread_t monitor_thread;

struct MonitorArgs {
    char controller_ip[INET_ADDRSTRLEN];
    int udp_port;
};

struct ClientArgs {
    int client_fd;
    struct sockaddr_in client_addr;
};

/* =========================================================
   LOGGING
   ========================================================= */

void write_log(const char *message) {

    FILE *fp = fopen(LOG_FILE, "a");

    if (fp == NULL) {
        return;
    }

    time_t now = time(NULL);

    struct tm *t = localtime(&now);

    char time_buffer[64];

    strftime(
        time_buffer,
        sizeof(time_buffer),
        "%Y-%m-%d %H:%M:%S",
        t
    );

    fprintf(
        fp,
        "[%s] %s\n",
        time_buffer,
        message
    );

    fclose(fp);
}


/* =========================================================
   RECEIVE ONE TEXT LINE
   ========================================================= */

int receive_line(int sock, char *buffer, int size) {

    int i = 0;
    char ch;

    while (i < size - 1) {

        int n = recv(
            sock,
            &ch,
            1,
            0
        );

        if (n <= 0) {
            return n;
        }

        buffer[i++] = ch;

        if (ch == '\n') {
            break;
        }
    }

    buffer[i] = '\0';

    return i;
}


/* =========================================================
   SYSINFO
   ========================================================= */

void send_sysinfo(int client_fd) {

    double cpu_load = 0.0;

    FILE *fp = fopen(
        "/proc/loadavg",
        "r"
    );

    if (fp != NULL) {

        fscanf(
            fp,
            "%lf",
            &cpu_load
        );

        fclose(fp);
    }

    struct sysinfo info;

    if (sysinfo(&info) == 0) {

        unsigned long total_ram =
            info.totalram * info.mem_unit;

        unsigned long free_ram =
            info.freeram * info.mem_unit;

        unsigned long used_ram =
            total_ram - free_ram;

        unsigned long used_mb =
            used_ram / (1024 * 1024);

        long uptime =
            info.uptime;

        char response[256];

        snprintf(
            response,
            sizeof(response),
            "OK SYSINFO %.2f %lu %ld SID:%s\n",
            cpu_load,
            used_mb,
            uptime,
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        printf(
            "SYSINFO sent to Controller.\n"
        );
    }
}


/* =========================================================
   LISTPROC
   ========================================================= */

void send_listproc(int client_fd) {

    FILE *fp;

    char process_output[2048] = "";

    char line[256];

    fp = popen(
        "ps -eo pid,comm --no-headers | head -n 15",
        "r"
    );

    if (fp == NULL) {

        char response[128];

        snprintf(
            response,
            sizeof(response),
            "ERR 006 PROCESS_LIST_FAILED SID:%s\n",
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        return;
    }

    while (
        fgets(
            line,
            sizeof(line),
            fp
        ) != NULL
    ) {

        line[
            strcspn(
                line,
                "\n"
            )
        ] = '\0';

        if (
            strlen(process_output) > 0
        ) {

            strncat(
                process_output,
                ", ",
                sizeof(process_output)
                    - strlen(process_output)
                    - 1
            );
        }

        strncat(
            process_output,
            line,
            sizeof(process_output)
                - strlen(process_output)
                - 1
        );
    }

    pclose(fp);

    char response[2300];

    snprintf(
        response,
        sizeof(response),
        "OK PROCS %s SID:%s\n",
        process_output,
        SID
    );

    send(
        client_fd,
        response,
        strlen(response),
        0
    );

    printf(
        "LISTPROC sent to Controller.\n"
    );
}


/* =========================================================
   EXEC
   ========================================================= */

void handle_exec(
    int client_fd,
    const char *command
) {

    const char *shell_command = NULL;

    if (
        strcmp(
            command,
            "DATE"
        ) == 0
    ) {

        shell_command = "date";

    } else if (
        strcmp(
            command,
            "UPTIME"
        ) == 0
    ) {

        shell_command = "uptime";

    } else if (
        strcmp(
            command,
            "DISKFREE"
        ) == 0
    ) {

        shell_command = "df -h /";

    } else if (
        strcmp(
            command,
            "HOSTNAME"
        ) == 0
    ) {

        shell_command = "hostname";

    } else if (
        strcmp(
            command,
            "WHOAMI"
        ) == 0
    ) {

        shell_command = "whoami";

    } else {

        char response[128];

        snprintf(
            response,
            sizeof(response),
            "ERR 002 COMMAND_NOT_ALLOWED SID:%s\n",
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        printf(
            "EXEC command rejected: %s\n",
            command
        );

        return;
    }

    FILE *fp = popen(
        shell_command,
        "r"
    );

    if (fp == NULL) {

        char response[128];

        snprintf(
            response,
            sizeof(response),
            "ERR 007 EXEC_FAILED SID:%s\n",
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        return;
    }

    char output[1024] = "";

    char line[256];

    while (
        fgets(
            line,
            sizeof(line),
            fp
        ) != NULL
    ) {

        line[
            strcspn(
                line,
                "\n"
            )
        ] = ' ';

        strncat(
            output,
            line,
            sizeof(output)
                - strlen(output)
                - 1
        );
    }

    pclose(fp);

    char response[1400];

    snprintf(
        response,
        sizeof(response),
        "OK EXEC_RESULT %s SID:%s\n",
        output,
        SID
    );

    send(
        client_fd,
        response,
        strlen(response),
        0
    );

    printf(
        "EXEC command completed: %s\n",
        command
    );
}


/* =========================================================
   PUT FILE
   ========================================================= */

void handle_put(
    int client_fd,
    const char *filename,
    long filesize
) {

    char filepath[512];

    snprintf(
        filepath,
        sizeof(filepath),
        "%s%s",
        STORAGE_DIR,
        filename
    );

    FILE *fp = fopen(
        filepath,
        "wb"
    );

    if (fp == NULL) {

        char response[128];

        snprintf(
            response,
            sizeof(response),
            "ERR 008 FILE_SAVE_FAILED SID:%s\n",
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        return;
    }

    char buffer[1024];

    long remaining =
        filesize;

    while (remaining > 0) {

        int to_receive;

        if (
            remaining >
            (long)sizeof(buffer)
        ) {

            to_receive =
                sizeof(buffer);

        } else {

            to_receive =
                (int)remaining;
        }

        int n = recv(
            client_fd,
            buffer,
            to_receive,
            0
        );

        if (n <= 0) {
            break;
        }

        fwrite(
            buffer,
            1,
            n,
            fp
        );

        remaining -= n;
    }

    fclose(fp);

    if (remaining == 0) {

        char response[256];

        snprintf(
            response,
            sizeof(response),
            "OK FILE_RECEIVED %s SID:%s\n",
            filename,
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        printf(
            "File received successfully: %s\n",
            filename
        );

        char put_log[512];

        snprintf(
            put_log,
            sizeof(put_log),
            "File uploaded: %s (%ld bytes)",
            filename,
            filesize
        );

        write_log(put_log);

    } else {

        printf(
            "File transfer interrupted.\n"
        );
    }
}


/* =========================================================
   GET FILE
   ========================================================= */

void handle_get(
    int client_fd,
    const char *filename
) {

    char filepath[512];

    snprintf(
        filepath,
        sizeof(filepath),
        "%s%s",
        STORAGE_DIR,
        filename
    );

    FILE *fp = fopen(
        filepath,
        "rb"
    );

    if (fp == NULL) {

        char response[128];

        snprintf(
            response,
            sizeof(response),
            "ERR 005 FILE_NOT_FOUND SID:%s\n",
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        printf(
            "GET failed. File not found: %s\n",
            filename
        );

        return;
    }

    fseek(
        fp,
        0,
        SEEK_END
    );

    long filesize =
        ftell(fp);

    rewind(fp);

    char response[256];

    snprintf(
        response,
        sizeof(response),
        "OK FILE_SEND %s %ld SID:%s\n",
        filename,
        filesize,
        SID
    );

    send(
        client_fd,
        response,
        strlen(response),
        0
    );

    char buffer[1024];

    size_t bytes_read;

    while (
        (bytes_read =
            fread(
                buffer,
                1,
                sizeof(buffer),
                fp
            )
        ) > 0
    ) {

        send(
            client_fd,
            buffer,
            bytes_read,
            0
        );
    }

    fclose(fp);

    printf(
        "File sent successfully: %s\n",
        filename
    );

    char get_log[512];

    snprintf(
        get_log,
        sizeof(get_log),
        "File downloaded: %s (%ld bytes)",
        filename,
        filesize
    );

    write_log(get_log);
}


/* =========================================================
   UDP MONITORING THREAD
   ========================================================= */

void *monitor_function(void *arg) {

    struct MonitorArgs *args =
        (struct MonitorArgs *)arg;

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

        free(args);

        return NULL;
    }

    memset(
        &udp_addr,
        0,
        sizeof(udp_addr)
    );

    udp_addr.sin_family =
        AF_INET;

    udp_addr.sin_port =
        htons(args->udp_port);

    inet_pton(
        AF_INET,
        args->controller_ip,
        &udp_addr.sin_addr
    );

    printf(
        "UDP monitoring started to %s:%d\n",
        args->controller_ip,
        args->udp_port
    );

    while (monitor_running) {

        double cpu_load =
            0.0;

        FILE *fp =
            fopen(
                "/proc/loadavg",
                "r"
            );

        if (fp != NULL) {

            fscanf(
                fp,
                "%lf",
                &cpu_load
            );

            fclose(fp);
        }

        struct sysinfo info;

        unsigned long used_mb =
            0;

        long uptime =
            0;

        if (
            sysinfo(&info) == 0
        ) {

            unsigned long total_ram =
                info.totalram *
                info.mem_unit;

            unsigned long free_ram =
                info.freeram *
                info.mem_unit;

            unsigned long used_ram =
                total_ram -
                free_ram;

            used_mb =
                used_ram /
                (1024 * 1024);

            uptime =
                info.uptime;
        }

        char message[256];

        snprintf(
            message,
            sizeof(message),
            "SYSINFO %.2f %lu %ld SID:%s",
            cpu_load,
            used_mb,
            uptime,
            SID
        );

        sendto(
            udp_sock,
            message,
            strlen(message),
            0,
            (struct sockaddr *)
                &udp_addr,
            sizeof(udp_addr)
        );

        printf(
            "UDP monitor sent: %s\n",
            message
        );

        sleep(2);
    }

    printf(
        "UDP monitoring stopped.\n"
    );

    close(udp_sock);

    free(args);

    return NULL;
}

void *handle_client(void *arg) {

    struct ClientArgs *client_args =
        (struct ClientArgs *)arg;

    int client_fd =
        client_args->client_fd;

    struct sockaddr_in client_addr =
        client_args->client_addr;

    free(client_args);

    printf(
        "Controller connected from %s\n",
        inet_ntoa(client_addr.sin_addr)
    );

    char connection_log[256];

    snprintf(
        connection_log,
        sizeof(connection_log),
        "Controller connected from %s",
        inet_ntoa(client_addr.sin_addr)
    );

    write_log(connection_log);

    int authenticated = 0;

    char buffer[1024];

    while (1) {

        memset(
            buffer,
            0,
            sizeof(buffer)
        );

        int n = receive_line(
            client_fd,
            buffer,
            sizeof(buffer)
        );

        if (n <= 0) {

            printf(
                "Controller disconnected.\n"
            );

            write_log(
                "Controller disconnected"
            );

            break;
        }

        printf(
            "Received: %s",
            buffer
        );

        char command_log[1200];

        if (
            strncmp(
                buffer,
                "AUTH ",
                5
            ) == 0
        ) {

            snprintf(
                command_log,
                sizeof(command_log),
                "Command received: AUTH"
            );

        } else {

            snprintf(
                command_log,
                sizeof(command_log),
                "Command received: %s",
                buffer
            );

            command_log[
                strcspn(
                    command_log,
                    "\n"
                )
            ] = '\0';
        }

        write_log(command_log);

        if (!authenticated) {

            char correct_auth[100];

            snprintf(
                correct_auth,
                sizeof(correct_auth),
                "AUTH %s\n",
                AUTH_TOKEN
            );

            if (
                strcmp(
                    buffer,
                    correct_auth
                ) == 0
            ) {

                char response[100];

                snprintf(
                    response,
                    sizeof(response),
                    "OK AUTHENTICATED SID:%s\n",
                    SID
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );

                authenticated = 1;

                printf(
                    "Authentication successful.\n"
                );

                write_log(
                    "Authentication successful"
                );

            } else {

                char response[100];

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 001 AUTH_FAILED SID:%s\n",
                    SID
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );

                printf(
                    "Authentication failed.\n"
                );

                write_log(
                    "Authentication failed"
                );
            }

            continue;
        }

        if (
            strcmp(
                buffer,
                "SYSINFO\n"
            ) == 0
        ) {

            send_sysinfo(client_fd);
            continue;
        }

        if (
            strcmp(
                buffer,
                "LISTPROC\n"
            ) == 0
        ) {

            send_listproc(client_fd);
            continue;
        }

        if (
            strncmp(
                buffer,
                "EXEC ",
                5
            ) == 0
        ) {

            char exec_name[100];

            if (
                sscanf(
                    buffer,
                    "EXEC %99s",
                    exec_name
                ) == 1
            ) {

                handle_exec(
                    client_fd,
                    exec_name
                );
            }

            continue;
        }

        if (
            strncmp(
                buffer,
                "PUT ",
                4
            ) == 0
        ) {

            char filename[256];
            long filesize;

            if (
                sscanf(
                    buffer,
                    "PUT %255s %ld",
                    filename,
                    &filesize
                ) == 2
            ) {

                handle_put(
                    client_fd,
                    filename,
                    filesize
                );
            }

            continue;
        }

        if (
            strncmp(
                buffer,
                "GET ",
                4
            ) == 0
        ) {

            char filename[256];

            if (
                sscanf(
                    buffer,
                    "GET %255s",
                    filename
                ) == 1
            ) {

                handle_get(
                    client_fd,
                    filename
                );
            }

            continue;
        }

        if (
            strcmp(
                buffer,
                "QUIT\n"
            ) == 0
        ) {

            char response[128];

            snprintf(
                response,
                sizeof(response),
                "OK BYE SID:%s\n",
                SID
            );

            send(
                client_fd,
                response,
                strlen(response),
                0
            );

            printf(
                "QUIT received. Closing connection.\n"
            );

            write_log(
                "Client requested graceful disconnect"
            );

            break;
        }

        char response[128];

        snprintf(
            response,
            sizeof(response),
            "ERR 003 UNKNOWN_COMMAND SID:%s\n",
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );
    }

    close(client_fd);

    return NULL;
}


/* =========================================================
   MAIN
   ========================================================= */

int main() {

    int server_fd;

    int client_fd;

    struct sockaddr_in server_addr;

    struct sockaddr_in client_addr;

    socklen_t client_len =
        sizeof(client_addr);

    server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd < 0) {

        perror(
            "Socket creation failed"
        );

        return 1;
    }

    int opt = 1;

    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );

    printf(
        "Socket created successfully.\n"
    );

    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);

    if (
        bind(
            server_fd,
            (struct sockaddr *)
                &server_addr,
            sizeof(server_addr)
        ) < 0
    ) {

        perror("Bind failed");

        close(server_fd);

        return 1;
    }

    printf(
        "Agent bound to port %d.\n",
        PORT
    );

    if (
        listen(
            server_fd,
            5
        ) < 0
    ) {

        perror("Listen failed");

        close(server_fd);

        return 1;
    }

    printf(
        "RemoteOps Agent is listening on port %d...\n",
        PORT
    );
  while (1) {

    struct sockaddr_in client_addr;

    socklen_t client_len =
        sizeof(client_addr);

    int client_fd =
        accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len
        );

    if (client_fd < 0) {

        perror("Accept failed");
        continue;
    }

    struct ClientArgs *args =
        malloc(
            sizeof(
                struct ClientArgs
            )
        );

    args->client_fd =
        client_fd;

    args->client_addr =
        client_addr;

    pthread_t client_thread;

    if (
        pthread_create(
            &client_thread,
            NULL,
            handle_client,
            args
        ) != 0
    ) {

        perror(
            "Failed to create client thread"
        );

        close(client_fd);
        free(args);

        continue;
    }

    pthread_detach(
        client_thread
    );
}

    client_fd = accept(
        server_fd,
        (struct sockaddr *)
            &client_addr,
        &client_len
    );

    if (client_fd < 0) {

        perror("Accept failed");

        close(server_fd);

        return 1;
    }

    printf(
        "Controller connected from %s\n",
        inet_ntoa(
            client_addr.sin_addr
        )
    );

    char connection_log[256];

    snprintf(
        connection_log,
        sizeof(connection_log),
        "Controller connected from %s",
        inet_ntoa(
            client_addr.sin_addr
        )
    );

    write_log(
        connection_log
    );


    int authenticated = 0;

    char buffer[1024];


    while (1) {

        memset(
            buffer,
            0,
            sizeof(buffer)
        );

        int n = receive_line(
            client_fd,
            buffer,
            sizeof(buffer)
        );

        if (n <= 0) {

            printf(
                "Controller disconnected.\n"
            );

            write_log(
                "Controller disconnected"
            );

            break;
        }

        printf(
            "Received: %s",
            buffer
        );


        /* Log command */

        char command_log[1200];

        if (
            strncmp(
                buffer,
                "AUTH ",
                5
            ) == 0
        ) {

            snprintf(
                command_log,
                sizeof(command_log),
                "Command received: AUTH"
            );

        } else {

            snprintf(
                command_log,
                sizeof(command_log),
                "Command received: %s",
                buffer
            );

            command_log[
                strcspn(
                    command_log,
                    "\n"
                )
            ] = '\0';
        }

        write_log(
            command_log
        );


        /* =================================================
           AUTH
           ================================================= */

        if (!authenticated) {

            char correct_auth[100];

            snprintf(
                correct_auth,
                sizeof(correct_auth),
                "AUTH %s\n",
                AUTH_TOKEN
            );

            if (
                strcmp(
                    buffer,
                    correct_auth
                ) == 0
            ) {

                char response[100];

                snprintf(
                    response,
                    sizeof(response),
                    "OK AUTHENTICATED SID:%s\n",
                    SID
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );

                authenticated =
                    1;

                printf(
                    "Authentication successful.\n"
                );

                write_log(
                    "Authentication successful"
                );

            } else {

                char response[100];

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 001 AUTH_FAILED SID:%s\n",
                    SID
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );

                printf(
                    "Authentication failed.\n"
                );

                write_log(
                    "Authentication failed"
                );
            }

            continue;
        }


        /* =================================================
           SYSINFO
           ================================================= */

        if (
            strcmp(
                buffer,
                "SYSINFO\n"
            ) == 0
        ) {

            send_sysinfo(
                client_fd
            );

            continue;
        }


        /* =================================================
           LISTPROC
           ================================================= */

        if (
            strcmp(
                buffer,
                "LISTPROC\n"
            ) == 0
        ) {

            send_listproc(
                client_fd
            );

            continue;
        }


        /* =================================================
           EXEC
           ================================================= */

        if (
            strncmp(
                buffer,
                "EXEC ",
                5
            ) == 0
        ) {

            char exec_name[100];

            if (
                sscanf(
                    buffer,
                    "EXEC %99s",
                    exec_name
                ) == 1
            ) {

                handle_exec(
                    client_fd,
                    exec_name
                );
            }

            continue;
        }


        /* =================================================
           PUT
           ================================================= */

        if (
            strncmp(
                buffer,
                "PUT ",
                4
            ) == 0
        ) {

            char filename[256];

            long filesize;

            if (
                sscanf(
                    buffer,
                    "PUT %255s %ld",
                    filename,
                    &filesize
                ) == 2
            ) {

                printf(
                    "Receiving file: %s (%ld bytes)\n",
                    filename,
                    filesize
                );

                handle_put(
                    client_fd,
                    filename,
                    filesize
                );

            } else {

                char response[128];

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 009 INVALID_PUT SID:%s\n",
                    SID
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );
            }

            continue;
        }


        /* =================================================
           GET
           ================================================= */

        if (
            strncmp(
                buffer,
                "GET ",
                4
            ) == 0
        ) {

            char filename[256];

            if (
                sscanf(
                    buffer,
                    "GET %255s",
                    filename
                ) == 1
            ) {

                printf(
                    "GET request for: %s\n",
                    filename
                );

                handle_get(
                    client_fd,
                    filename
                );

            } else {

                char response[128];

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 010 INVALID_GET SID:%s\n",
                    SID
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );
            }

            continue;
        }


        /* =================================================
           MONITOR START
           ================================================= */

        if (
            strncmp(
                buffer,
                "MONITOR START ",
                14
            ) == 0
        ) {

            int udp_port;

            if (
                sscanf(
                    buffer,
                    "MONITOR START %d",
                    &udp_port
                ) == 1
            ) {

                if (
                    !monitor_running
                ) {

                    monitor_running =
                        1;

                    struct MonitorArgs *args =
                        malloc(
                            sizeof(
                                struct MonitorArgs
                            )
                        );

                    strcpy(
                        args->controller_ip,
                        inet_ntoa(
                            client_addr.sin_addr
                        )
                    );

                    args->udp_port =
                        udp_port;

                    pthread_create(
                        &monitor_thread,
                        NULL,
                        monitor_function,
                        args
                    );

                    char response[128];

                    snprintf(
                        response,
                        sizeof(response),
                        "OK MONITOR_STARTED SID:%s\n",
                        SID
                    );

                    send(
                        client_fd,
                        response,
                        strlen(response),
                        0
                    );

                    printf(
                        "MONITOR START command accepted.\n"
                    );

                    write_log(
                        "UDP monitoring started"
                    );
                }
            }

            continue;
        }


        /* =================================================
           MONITOR STOP
           ================================================= */

        if (
            strcmp(
                buffer,
                "MONITOR STOP\n"
            ) == 0
        ) {

            if (
                monitor_running
            ) {

                monitor_running =
                    0;

                pthread_join(
                    monitor_thread,
                    NULL
                );
            }

            char response[128];

            snprintf(
                response,
                sizeof(response),
                "OK MONITOR_STOPPED SID:%s\n",
                SID
            );

            send(
                client_fd,
                response,
                strlen(response),
                0
            );

            printf(
                "MONITOR STOP command accepted.\n"
            );

            write_log(
                "UDP monitoring stopped"
            );

            continue;
        }


        /* =================================================
           QUIT
           ================================================= */

        if (
            strcmp(
                buffer,
                "QUIT\n"
            ) == 0
        ) {

            if (
                monitor_running
            ) {

                monitor_running =
                    0;

                pthread_join(
                    monitor_thread,
                    NULL
                );
            }

            char response[128];

            snprintf(
                response,
                sizeof(response),
                "OK BYE SID:%s\n",
                SID
            );

            send(
                client_fd,
                response,
                strlen(response),
                0
            );

            printf(
                "QUIT received. Closing connection.\n"
            );

            write_log(
                "Client requested graceful disconnect"
            );

            break;
        }


        /* =================================================
           UNKNOWN COMMAND
           ================================================= */

        char response[128];

        snprintf(
            response,
            sizeof(response),
            "ERR 003 UNKNOWN_COMMAND SID:%s\n",
            SID
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );
    }


    if (monitor_running) {

        monitor_running = 0;

        pthread_join(
            monitor_thread,
            NULL
        );
    }


    close(client_fd);

    close(server_fd);

    return 0;
}
