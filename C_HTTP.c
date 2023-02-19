#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <process.h>

#define BUFFER_SIZE 2048
#define SMALL_BUFFER 200

void printError(const char* message);
unsigned WINAPI clientHandler(void* arg);
void sendMessage(SOCKET sock, const char* errorType, char* filePath);
void sendError(SOCKET sock, const char* errorType);
void sendError404(SOCKET sock);
void sendError400(SOCKET sock);
int whatMethod(SOCKET sock, char* method);
void dateSet(char* date);

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	SOCKET sSocket, cSocket;
	SOCKADDR_IN sAddress, cAddress;
	HANDLE thread;
	int clntAddrSize, port;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		printError("WSAStartup() func error");

	printf("Input port : ");
	scanf("%d", &port);
	if (port < 1024 || port > 49151)
		printError("Port error");

	if ((sSocket = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		printError("socket() func error");
	memset(&sAddress, 0, sizeof(sAddress));
	sAddress.sin_family = AF_INET;
	sAddress.sin_port = htons((unsigned short)port);
	sAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sSocket, (SOCKADDR*)&sAddress, sizeof(sAddress)) == SOCKET_ERROR)
	{
		closesocket(sSocket);
		printError("bind() func error");
	}
	if (listen(sSocket, 5) == SOCKET_ERROR)
	{
		closesocket(sSocket);
		printError("listen() func error");
	}

	while (1)
	{
		clntAddrSize = sizeof(cAddress);
		if ((cSocket = accept(sSocket, (SOCKADDR*)&cAddress, &clntAddrSize)) == INVALID_SOCKET)
		{
			closesocket(sSocket);
			printError("accept() func error");
		}
		printf("Client Connection : %s:%d\n",
			inet_ntoa(cAddress.sin_addr), ntohs(cAddress.sin_port));
		thread = (HANDLE)_beginthreadex(NULL, 0, clientHandler, (void*)cSocket, 0, NULL);
	}
	closesocket(sSocket);
	WSACleanup();
	return 0;
}

unsigned WINAPI clientHandler(void* arg)
{
	SOCKET sock = (SOCKET)arg;
	char msg[BUFFER_SIZE];
	char method[SMALL_BUFFER];
	char filePath[SMALL_BUFFER];
	char proto[SMALL_BUFFER];

	recv(sock, msg, BUFFER_SIZE, 0);
	strcpy(method, strtok(msg, " /"));
	strcpy(filePath, strtok(NULL, " "));

	if (strcmp(filePath, "/") == 0)
		strcpy(filePath, "index.html");
	else
		strcpy(filePath, &filePath[1]);
	strcpy(proto, strtok(NULL, " "));

	if (strstr(proto, "HTTP/") == NULL)
	{
		sendError400(sock);
		closesocket(sock);
		return 1;
	}

	if (whatMethod(sock, method) == 0)
		sendMessage(sock, "200 OK", filePath);

	closesocket(sock);
	return 0;
}

int whatMethod(SOCKET sock, char* method)
{
	if (strcmp(method, "GET") == 0)
		return 0;
	sendError400(sock);
	return -1;
}

void sendMessage(SOCKET sock, const char* status, char* filePath)
{
	char proto[SMALL_BUFFER];
	char date[SMALL_BUFFER];
	char name[] = "Server: SOULmos\r\n";
	char allow[] = "Allow: GET\r\n";
	char length[] = "Content-length: 2048\r\n";
	char type[] = "Content-type: text/html\r\n\r\n";
	char buf[BUFFER_SIZE];
	FILE* fp;

	sprintf(proto, "HTTP/1.1 %s\r\n", status);
	dateSet(date);

	if ((fp = fopen(filePath, "r")) == NULL)
	{
		sendError404(sock);
		return;
	}

	send(sock, proto, strlen(proto), 0);
	send(sock, date, strlen(date), 0);
	send(sock, name, strlen(name), 0);
	send(sock, allow, strlen(allow), 0);
	send(sock, length, strlen(length), 0);
	send(sock, type, strlen(type), 0);
	while (fgets(buf, BUFFER_SIZE, fp) != NULL)
		send(sock, buf, strlen(buf), 0);
	closesocket(sock);
}

void sendError404(SOCKET sock) { sendError(sock, "404 Not Found"); }
void sendError400(SOCKET sock) { sendError(sock, "400 Bad Request"); }
void sendError(SOCKET sock, const char* errorType)
{
	char proto[SMALL_BUFFER];
	char name[] = "Server: SOULmos\r\n";
	char date[SMALL_BUFFER];
	char length[] = "Content-length: 2048\r\n";
	char type[] = "Content-type: text/html\r\n\r\n";
	char content[BUFFER_SIZE];

	sprintf(proto, "HTTP/1.1 %s\r\n", errorType);
	sprintf(content, "<html><head><title>Response ERROR</title></head>"
		"<body><font size=+5><br>%s"
		"</font></body></html>", errorType);
	dateSet(date);

	send(sock, proto, strlen(proto), 0);
	send(sock, date, strlen(date), 0);
	send(sock, name, strlen(name), 0);
	send(sock, length, strlen(length), 0);
	send(sock, type, strlen(type), 0);
	send(sock, content, strlen(content), 0);

	closesocket(sock);
}

void dateSet(char* date)
{
	char dow[][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	char mon[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	char buf[SMALL_BUFFER];
	time_t timer;
	struct tm* t;
	timer = time(NULL);
	t = localtime(&timer);
	sprintf(buf, "Date: %s, %d %s %d %2d:%2d:%2d GMT\r\n",
		dow[t->tm_wday], t->tm_mday, mon[t->tm_mon], t->tm_year, t->tm_hour, t->tm_min, t->tm_sec);
	strcpy(date, buf);
}

void printError(const char* message)
{
	fprintf(stderr, "%s\n", message);
	exit(1);
}
