readme ekt meka dammoth harida?   REMOTEOPS – REMOTE SYSTEM MONITORING AND MANAGEMENT TOOL

Module Information
Module: IE3090 – Network Programming
Assignment: RemoteOps: A Remote System Monitoring and Management Tool over TCP/IP
Registration Number: IT24100623

PERSONALISED VALUES

Agent TCP Port: 9410
Session ID (SID): 3260
Authentication Token: OPS-0623
Agent Source File: agent_623.c
Controller Source File: controller_623.c
Makefile: Makefile_623
Log File: remoteops_IT24100623.log
Storage Path: ./agentfiles/IT24100623/
Submission ZIP: IE3090_IT24100623.zip


PROJECT OVERVIEW

RemoteOps is a client/server system written in C using the standard BSD sockets API.

The Agent runs on the managed machine and listens for TCP Controller connections.

The Controller connects to the Agent, authenticates, and sends the commands defined in the assignment protocol.

TCP is used for authentication, commands, responses, and file transfer.

UDP is used for periodic system monitoring.

The Agent uses POSIX threads so that multiple Controller connections can be handled concurrently.


IMPLEMENTED FEATURES

1. TCP CONNECTION

The Agent listens on the personalised TCP port 9410 and accepts Controller connections.


2. AUTHENTICATION

The first command on a new connection must be:

AUTH OPS-0623

Successful response:

OK AUTHENTICATED SID:3260

An invalid authentication token returns:

ERR 001 AUTH_FAILED SID:3260


3. SYSINFO

The SYSINFO command returns:

CPU load
Used memory in MB
System uptime in seconds
Personalised SID

Example format:

OK SYSINFO <cpu_load> <mem_used_mb> <uptime_sec> SID:3260


4. LISTPROC

The LISTPROC command returns a snapshot of running processes.

Example format:

OK PROCS <process list> SID:3260


5. EXEC

Only the following commands are allowed:

DATE
UPTIME
DISKFREE
HOSTNAME
WHOAMI

Any other EXEC command is rejected with:

ERR 002 COMMAND_NOT_ALLOWED SID:3260


6. PUT – FILE UPLOAD

The Controller can upload a file using:

PUT <filename> <filesize>

The Agent stores uploaded files under:

./agentfiles/IT24100623/

Successful response:

OK FILE_RECEIVED <filename> SID:3260


7. GET – FILE DOWNLOAD

The Controller can request a previously uploaded file using:

GET <filename>

Successful response format:

OK FILE_SEND <filename> <filesize> SID:3260

The file bytes are then transferred to the Controller.

If the file does not exist:

ERR 005 FILE_NOT_FOUND SID:3260


8. UDP MONITORING

The Controller starts monitoring using:

MONITOR START <udp_port>

Successful response:

OK MONITOR_STARTED SID:3260

The Agent then sends periodic UDP system-statistics datagrams in this format:

SYSINFO <cpu_load> <mem_used_mb> <uptime_sec> SID:3260

The monitoring interval used in this implementation is 2 seconds.

Monitoring is stopped using:

MONITOR STOP

Response:

OK MONITOR_STOPPED SID:3260


9. QUIT

The QUIT command cleanly closes the connection.

Response:

OK BYE SID:3260


10. LOGGING

The Agent records connections, received commands, file transfers, monitoring activity, authentication events, and disconnect events with timestamps in:

remoteops_IT24100623.log


11. CONCURRENCY

The Agent uses POSIX threads.

Each accepted Controller connection is handled by a separate client thread so that multiple Controllers can be served simultaneously.


PROJECT STRUCTURE

RemoteOps/

agent_623.c
controller_623.c
Makefile_623
remoteops_IT24100623.log
test.txt
downloaded_test.txt

agentfiles/
    IT24100623/
        test.txt


COMPILATION

Compile both programs using the personalised Makefile:

make -f Makefile_623

Alternatively, compile manually:

gcc agent_623.c -o agent_623 -pthread
gcc controller_623.c -o controller_623


RUNNING THE PROGRAM

Start the Agent:

./agent_623

The Agent should display:

Socket created successfully.
Agent bound to port 9410.
RemoteOps Agent is listening on port 9410...


Start the Controller:

./controller_623

The Controller connects to the Agent and performs the implemented RemoteOps protocol operations.


CHECKING THE PERSONALISED LISTENING PORT

While the Agent is running:

ss -tlnp | grep 9410


CHECKING UPLOADED FILES

ls -l agentfiles/IT24100623/


CHECKING THE LOG FILE

cat remoteops_IT24100623.log


FILE INTEGRITY TEST

To verify that a downloaded file is byte-for-byte identical to the original:

cmp test.txt downloaded_test.txt

If cmp produces no output, the files are identical.


CONCURRENCY TEST

To test multiple simultaneous Controller connections, keep the Agent running and open multiple terminals.

In each terminal:

nc 127.0.0.1 9410

Then authenticate with:

AUTH OPS-0623

The Agent should accept and authenticate multiple simultaneous Controller connections.


DESIGN NOTES

The project is implemented entirely in C.

Standard BSD TCP and UDP sockets are used.

POSIX threads are used for concurrent Controller handling.

TCP text commands are newline-terminated.

PUT and GET transfer exactly the announced number of file bytes.

Remote command execution is restricted to the fixed whitelist required by the assignment.

UDP monitoring uses a 2-second interval.

Every TCP response includes the personalised SID tag.


AUTHOR

Registration Number: IT24100623
