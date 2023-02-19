/* 윈도우 환경에서 HTTP 서버 구축하기 */

/* Visual studio 2022 환경에서 작업하기 위해 추가한 매크로
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <process.h>

#define BUFFER_SIZE 2048
#define SMALL_BUFFER 200

/* 함수 선언 목록 */
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
	WSADATA wsaData;				// winsock 초기화를 위해 만든 변수
	SOCKET sSocket, cSocket;		// 서버소켓과 통신을위한 클라이언트소켓 생성
	SOCKADDR_IN sAddress, cAddress;	// 서버와 클라이언트의 주소정보(IP, PORT)저장 위한 변수
	HANDLE thread;
	int clntAddrSize, port;

	/* winsock 라이브러리(ws2_32.dll)를 초기화하는 역할, 실패하면 메모리에 로드되지 않아서 winsock 사용 불가능 */
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		printError("WSAStartup() func error");

	printf("Input port : ");
	scanf("%d", &port);
	if (port < 1024 || port > 49151)
		printError("Port error");
	// 잘못된 범위의 포트번호 설정시 에러
	// 0 ~ 1023는 예약된 포트번호, 49151 ~ 65535는 동적포트로 사용하면 원치않은 결과 나올 수 있음

	/* 소켓을 생성하는 과정, SOCK_STREAM인자를 줬으므로 TCP소켓 */
	if ((sSocket = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		printError("socket() func error");

	/* 서버 주소(IP, PORT)를 소켓에 할당 */
	memset(&sAddress, 0, sizeof(sAddress));				// 반드시 0으로 SOCKADDR_IN 구조체를 초기화해줘야 함
	sAddress.sin_family = AF_INET;						// IPv4방식 사용
	sAddress.sin_port = htons((unsigned short)port);	// PORT번호 설정
	sAddress.sin_addr.s_addr = htonl(INADDR_ANY);		// IP주소 설정(localhost)
	if (bind(sSocket, (SOCKADDR*)&sAddress, sizeof(sAddress)) == SOCKET_ERROR)
	{
		closesocket(sSocket);
		printError("bind() func error");
	}


	/* 서버소켓과 연결요청 대기큐를 만들고 연결요청 대기상태로 만듦 */
	if (listen(sSocket, 5) == SOCKET_ERROR)
	{
		closesocket(sSocket);
		printError("listen() func error");
	}

	/* 클라이언트가 연결요청을 하면 받아들이고 연결요청을 처리할 쓰레드 생성 */
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

	/* 소켓 닫기 및 winsock 종료 */
	closesocket(sSocket);
	WSACleanup();
	return 0;
}

/* 클라이언트 처리 쓰레드 */
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

	// IP와 PORT번호만 입력시 기본적으로 루트("/")경로 요청
	// 루트경로 요청하면 index.html출력하도록 설정
	if (strcmp(filePath, "/") == 0)
		strcpy(filePath, "index.html");
	else
		strcpy(filePath, &filePath[1]);
	strcpy(proto, strtok(NULL, " "));

	// HTTP프로토콜에 의한 요청이 아니면 에러출력
	if (strstr(proto, "HTTP/") == NULL)
	{
		sendError400(sock);
		closesocket(sock);
		return 1;
	}

	/* 구현된 메소드인지 확인 */
	if (whatMethod(sock, method) == 0)
		sendMessage(sock, "200 OK", filePath);

	closesocket(sock);
	return 0;
}

/* 요청한 메소드가 구현된 메소드인지 확인 */
/* 구현한 메소드가 GET뿐이므로, 다른 요청 들어오면 400에러 출력 */
int whatMethod(SOCKET sock, char* method)
{
	if (strcmp(method, "GET") == 0)
		return 0;
	sendError400(sock);
	return -1;
}

/* GET 메소드 정상입력시 Response 메세지 보내는 함수 */
void sendMessage(SOCKET sock, const char* status, char* filePath)
{
	// 상태라인 구현
	char proto[SMALL_BUFFER];
	// 메세지헤더 구현
	char date[SMALL_BUFFER];
	char name[] = "Server: SOULmos\r\n";
	char allow[] = "Allow: GET\r\n";
	char length[] = "Content-length: 2048\r\n";
	char type[] = "Content-type: text/html\r\n\r\n";
	char buf[BUFFER_SIZE];
	FILE* fp;

	sprintf(proto, "HTTP/1.1 %s\r\n", status);
	dateSet(date);

	/* 전송할 파일 오픈 */
	/* 존재하지 않는 파일이면 404에러 */
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
	// 메세지몸체 전송
	while (fgets(buf, BUFFER_SIZE, fp) != NULL)
		send(sock, buf, strlen(buf), 0);
	closesocket(sock);
}

/* 404에러 응답 함수 */
void sendError404(SOCKET sock) { sendError(sock, "404 Not Found"); }
/* 400에러 응답 함수 */
void sendError400(SOCKET sock) { sendError(sock, "400 Bad Request"); }

/* 에러 Response 메세지 보내는 함수 */
void sendError(SOCKET sock, const char* errorType)
{
	// 상태라인 구현
	char proto[SMALL_BUFFER];
	// 메세지헤더 구현
	char name[] = "Server: SOULmos\r\n";
	char date[SMALL_BUFFER];
	char length[] = "Content-length: 2048\r\n";
	char type[] = "Content-type: text/html\r\n\r\n";
	char content[BUFFER_SIZE];

	sprintf(proto, "HTTP/1.1 %s\r\n", errorType);
	// 에러출력할 메세지몸체 구현
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

/* 응답메세지 헤더에 Date 항목 설정 */
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

/* 에러 조작 함수 */
void printError(const char* message)
{
	fprintf(stderr, "%s\n", message);
	exit(1);
}
