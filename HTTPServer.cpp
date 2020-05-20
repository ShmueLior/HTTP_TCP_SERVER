#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
//TBD - might need to change includes

const int MAX_SIZE = 1024;
struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[MAX_SIZE];
	int len;
	time_t timeLastByte;
};

const int HTTP_PORT = 8080;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;

//TBD - might need to change the delete
const int OPTIONS = 1;
const int GET = 2;
const int HEAD = 3;
const int PUT = 4;
const int DELETE_METHOD = 5;
const int TRACE = 6;
const int EXIT = 7;
//const int DELETED_ERROR = -1;


bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
//TBD might need to add here more functions

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void main()
{
	time_t currTime;

	//for select
	struct timeval timeoutSelect;
	timeoutSelect.tv_sec = 120;
	timeoutSelect.tv_usec = 0;

	// Initialize Winsock (Windows Sockets).
	WSAData wsaData;

	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "HTTP Server: Error at WSAStartup()\n";
		return;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "HTTP Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.
    // Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(HTTP_PORT);

	// Bind the socket for client's requests.
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "HTTP Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	if (SOCKET_ERROR == listen(listenSocket, 5)) //TBD might need to change
	{
		cout << "HTTP Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);
	cout << "HTTP Server: Waiting for client connections" << endl;

	// Accept connections and handles them one by one.
	while (true)
	{
		
		//closing all sockets that last byte was more than 2 min ago
		for (int i = 1; i < MAX_SOCKETS; i++)
		{
			currTime = time(0);
			if ((currTime - sockets[i].timeLastByte > 120) && (sockets[i].timeLastByte != 0))
			{
				removeSocket(i);
			}
		}

		// The select function determines the status of one or more sockets
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		//select with timeout
		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, &timeoutSelect);
		if (nfd == SOCKET_ERROR)
		{
			cout << "HTTP Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i); //TBD - might need to change
					break;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "HTTP Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			sockets[i].timeLastByte= time(0);
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	sockets[index].timeLastByte = 0;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	sockets[index].timeLastByte = time(0);
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*) & from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "HTTP Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "HTTP Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	// Set the socket to be in non-blocking mode.
	//TBD - might need to change - might need to add to addsocket
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "HTTP Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "HTTP Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	sockets[index].timeLastByte = time(0);
	if (bytesRecv == 0) //the client closed the connection
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "HTTP Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = OPTIONS;
				memcpy(sockets[index].buffer, &sockets[index].buffer[7], sockets[index].len - 7);
				sockets[index].len -= 7;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			else if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = GET;
				memcpy(sockets[index].buffer, &sockets[index].buffer[3], sockets[index].len - 3);
				sockets[index].len -= 3;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = HEAD;
				memcpy(sockets[index].buffer, &sockets[index].buffer[4], sockets[index].len - 4);
				sockets[index].len -= 4;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = PUT;
				memcpy(sockets[index].buffer, &sockets[index].buffer[3], sockets[index].len - 3);
				sockets[index].len -= 3;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = TRACE;
				return;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = DELETE_METHOD;
				memcpy(sockets[index].buffer, &sockets[index].buffer[6], sockets[index].len - 6);
				sockets[index].len -= 6;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			//TBD - might need to add an option for non-implemented requests method
		}
	}

}

void sendMessage(int index)
{
	int bytesSent = 0;
	char sendBuff[MAX_SIZE];

	SOCKET msgSocket = sockets[index].id;
	if (sockets[index].sendSubType == OPTIONS)
	{
		optionMethod(sendBuff, index);
	}
	else if (sockets[index].sendSubType == GET || sockets[index].sendSubType == HEAD)
	{
		getOrHeadMethod(sendBuff, index);
	}
	else if (sockets[index].sendSubType == PUT)
	{
		putMethod(sendBuff, index);
	}
	else if (sockets[index].sendSubType == DELETE_METHOD)
	{
		deleteMethod(sendBuff, index);
	}
	else if (sockets[index].sendSubType == TRACE)
	{
		strcpy(sendBuff, "HTTP/1.1 200 OK \r\n");
		strcat(sendBuff, "Content-Length: 0\r\nContent-Type: text/html\r\n\r\n");
		strcat(sendBuff, sockets[index].buffer);
	}

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "HTTP Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "HTTP Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

	sockets[index].send = IDLE;
}
