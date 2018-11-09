#define WIN32_LEAN_AND_MEAN

#include "Message.h"

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

SOCKET getListenSocket()
{
	// Initialize Winsock
	WSADATA wsaData;
	auto ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0)
	{
		std::cout << "WSAStartup failed with error: " << ret << std::endl;
		return 0;
	}

	addrinfo addrInfoHints;
	ZeroMemory(&addrInfoHints, sizeof(addrInfoHints));
	addrInfoHints.ai_family = AF_INET;
	addrInfoHints.ai_socktype = SOCK_STREAM;
	addrInfoHints.ai_protocol = IPPROTO_TCP;
	addrInfoHints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	addrinfo* addrInfoResult = nullptr;
	ret = getaddrinfo(nullptr, "27015", &addrInfoHints, &addrInfoResult);
	if (ret != 0)
	{
		std::cout << "getaddrinfo failed with error: " << ret << std::endl;
		WSACleanup();
		return 0;
	}

	// Create a SOCKET for connecting to server
	auto listenSocket = INVALID_SOCKET;
	listenSocket = socket(addrInfoResult->ai_family, addrInfoResult->ai_socktype, addrInfoResult->ai_protocol);
	if (listenSocket == INVALID_SOCKET)
	{
		std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
		freeaddrinfo(addrInfoResult);
		WSACleanup();
		return 0;
	}

	// Setup the TCP listening socket
	ret = bind(listenSocket, addrInfoResult->ai_addr, (int)addrInfoResult->ai_addrlen);
	if (ret == SOCKET_ERROR)
	{
		std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
		freeaddrinfo(addrInfoResult);
		closesocket(listenSocket);
		WSACleanup();
		return 0;
	}

	freeaddrinfo(addrInfoResult);

	ret = listen(listenSocket, SOMAXCONN);
	if (ret == SOCKET_ERROR)
	{
		std::cout << "listen failed with error: " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return 0;
	}

	return listenSocket;
}

void listenToClient(SOCKET client)
{
	constexpr int bufferSize = 4096;
	std::array<char, bufferSize> buffer;
	size_t index = 0;
	int ret = 0;
	size_t invalidBytesCount = 0;

	std::cout << "[client: " << client << "] Opening connection" << std::endl;

	// Receive until the peer shuts down the connection
	while (true)
	{
		ret = recv(client, buffer.data() + index, 11, 0);
		if (ret > 0)
		{
			size_t bufferIndex = 0;
			index += ret;

			while (true)
			{
				const Message::Header* header = nullptr;

				while (true)
				{
					if (index < sizeof(Message::Header))
					{
						break;
					}

					auto tempHeader = reinterpret_cast<const Message::Header*>(buffer.data() + bufferIndex);

					if (tempHeader->m_magic != 0xDEAD1991FACE2018)
					{
						--index;
						++bufferIndex;
						++invalidBytesCount;
					}
					else
					{
						if (invalidBytesCount)
						{
							std::cout << "[client: " << client << "] Header found after " << invalidBytesCount << " invalid bytes" << std::endl;
						}
						header = tempHeader;
						break;
					}
				}

				if (header == nullptr or sizeof(Message::Header) + header->m_size > index)
				{
					break;
				}

				std::cout << "[client: " << client << "] ";

				switch (header->m_type)
				{
				case 0:
					std::cout << Message::parseCommand(buffer.data() + bufferIndex) << std::endl;
					index -= sizeof(Message::Header) + header->m_size;
					bufferIndex += sizeof(Message::Header) + header->m_size;
					break;
				case 1:
					std::cout << Message::parseLog(buffer.data() + bufferIndex) << std::endl;
					index -= sizeof(Message::Header) + header->m_size;
					bufferIndex += sizeof(Message::Header) + header->m_size;
					break;
				default:
					std::cout << "Unknown message type" << std::endl;
					break;
				}
			}
		}
		else if (ret == 0)
		{
			std::cout << "[client: " << client << "] Closing connection" << std::endl;
			break;
		}
		else
		{
			std::cout << "[client: " << client << "] Failed to recieve message" << std::endl;
			break;
		}
	}

	// shutdown the connection since we're done
	ret = shutdown(client, SD_SEND);
	if (ret == SOCKET_ERROR)
	{
		std::cout << "[client: " << client << "] Shutdown failed with error: " << WSAGetLastError() << std::endl;
	}

	// cleanup
	closesocket(client);
}

void acceptClients()
{
	auto listenSocket = getListenSocket();
	if (listenSocket == 0)
	{
		std::cout << "Failed to create listen socket" << std::endl;
		return;
	}

	while (true)
	{
		// Accept a client socket
		SOCKET client = INVALID_SOCKET;
		client = accept(listenSocket, nullptr, nullptr);
		if (client == INVALID_SOCKET)
		{
			std::cout << "accept failed with error: " << WSAGetLastError() << std::endl;
			continue;
		}

		std::thread clientThread(listenToClient, client);
		clientThread.detach();
	}

	// No longer need server socket
	closesocket(listenSocket);
}

int main(int argc, char** argv)
{
	acceptClients();

	return 0;
}