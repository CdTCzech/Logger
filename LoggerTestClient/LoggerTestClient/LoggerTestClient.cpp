#define WIN32_LEAN_AND_MEAN

#include <chrono>
#include <string>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

constexpr uint64_t magic = 0xDEAD1991FACE2018;

enum Level : uint8_t
{
	Off,
	Fatal,
	Error,
	Warning,
	Info,
	Communication,
	Debug,
	Verbose
};

#pragma pack(push, 1)
struct Header
{
	uint64_t m_magic;
	uint16_t m_type;
	uint32_t m_size;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Log
{
	uint64_t m_datetime;
	Level m_priority;
	uint16_t m_size;
};
#pragma pack(pop)

bool sendMessage(const SOCKET ConnectSocket, const char* buffer, size_t bufferIndex)
{
	// Send an initial buffer
	auto iResult = send(ConnectSocket, buffer, bufferIndex, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return false;
	}
	printf("Bytes Sent: %ld\n", iResult);
	return true;
}

int __cdecl main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	char sendbuf[] = "this is a test";
	char recvbuf[DEFAULT_BUFLEN];
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;

	// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	// !!! HANDSHAKE !!!

	// HELLO

	Header header;
	header.m_magic = 0xDEAD1991FACE2018;
	header.m_type = 0;
	header.m_size = 5;

	char buffer[4096];
	size_t bufferIndex = 0;

	std::string test = "H3LL0";

	memcpy(buffer, &header, sizeof(Header));
	bufferIndex = sizeof(Header);
	memcpy(buffer + bufferIndex, test.data(), test.size());
	bufferIndex += test.size();

	if (!sendMessage(ConnectSocket, buffer, bufferIndex))
	{
		return 1;
	}

	// WHO_ARE_U?

	auto ret = recv(ConnectSocket, buffer, sizeof(Header) + 10, 0);
	auto tempHeader = reinterpret_cast<const Header*>(buffer);
	if (ret <= 0 or tempHeader->m_magic != magic or tempHeader->m_type != 0 or tempHeader->m_size != 10)
	{
		return false;
	}
	std::string hello(buffer + sizeof(Header), 10);
	if (hello != "WHO_ARE_U?")
	{
		return false;
	}

	// YOUR_DAD

	test = "YOUR_DAD";
	header.m_size = 8;

	memcpy(buffer, &header, sizeof(Header));
	bufferIndex = sizeof(Header);
	memcpy(buffer + bufferIndex, test.data(), test.size());
	bufferIndex += test.size();

	if (!sendMessage(ConnectSocket, buffer, bufferIndex))
	{
		return 1;
	}

	// !!! SEND MESSAGES !!!

	// CORRECT INFO MESSAGE

	test = "A";

	Log log;
	header.m_type = 1;
	header.m_size = sizeof(Log) + test.size();
	log.m_datetime = std::chrono::system_clock::now().time_since_epoch().count();
	log.m_priority = Level::Info;
	log.m_size = test.size();

	memcpy(buffer, &header, sizeof(Header));
	bufferIndex = sizeof(Header);
	memcpy(buffer + bufferIndex, &log, sizeof(Log));
	bufferIndex += sizeof(Log);
	memcpy(buffer + bufferIndex, test.data(), test.size());
	bufferIndex += test.size();

	if (!sendMessage(ConnectSocket, buffer, bufferIndex))
	{
		return 1;
	}

	// WRONG MAGIC - 26 BYTES

	header.m_magic = 0x77771991FACE7777;
	memcpy(buffer, &header, sizeof(Header));

	if (!sendMessage(ConnectSocket, buffer, bufferIndex))
	{
		return 1;
	}

	// 50 CORRECT DEBUG MESSAGES

	for (int i = 0; i < 50; ++i)
	{
		std::string temp;
		for (auto j = 0; j < i; ++j)
		{
			temp += (rand() % 26) + 65;
		}

		header.m_magic = magic;

		bufferIndex = 0;
		test = "This should also get to you... " + temp;
		header.m_size = sizeof(Log) + test.size();
		log.m_datetime = std::chrono::system_clock::now().time_since_epoch().count();
		log.m_priority = Level::Debug;
		log.m_size = test.size();
		memcpy(buffer, &header, sizeof(Header));
		bufferIndex = sizeof(Header);
		memcpy(buffer + bufferIndex, &log, sizeof(Log));
		bufferIndex += sizeof(Log);
		memcpy(buffer + bufferIndex, test.data(), test.size());
		bufferIndex += test.size();

		if (!sendMessage(ConnectSocket, buffer, bufferIndex))
		{
			return 1;
		}
	}

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}