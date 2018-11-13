#define WIN32_LEAN_AND_MEAN

#include "Client.h"
#include "Message.h"

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

constexpr uint64_t magic = 0xDEAD1991FACE2018;
constexpr int bufferSize = 4096;


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

bool setTimeout(Client& client, int timeout)
{
	auto ret = setsockopt(client.m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
	if (ret != ERROR_SUCCESS)
	{
		std::cout << "setsockopt to " << timeout << " failed with error: " << WSAGetLastError() << std::endl;
		return false;
	}
	return true;
}

bool handshake(Client& client, std::array<char, bufferSize>& buffer)
{
	if (!setTimeout(client, 0/*10'000*/))
	{
		return false;
	}

	auto ret = recv(client.m_socket, buffer.data(), sizeof(Message::Header) + 5, 0);
	auto tempHeader = reinterpret_cast<const Message::Header*>(buffer.data());
	if (ret <= 0 or tempHeader->m_magic != magic or tempHeader->m_type != 0 or tempHeader->m_size != 5)
	{
		return false;
	}
	std::string hello(buffer.data() + sizeof(Message::Header), 5);
	if (hello != "H3LL0")
	{
		return false;
	}

	Message::Header header;
	header.m_magic = magic;
	header.m_type = 0;
	header.m_size = 10;
	int size = 0;
	memcpy(buffer.data(), &header, sizeof(Message::Header));
	size += sizeof(Message::Header);
	memcpy(buffer.data() + size, "WHO_ARE_U?", 10);
	size += 10;
	ret = send(client.m_socket, buffer.data(), size, 0);
	if (ret <= 0)
	{
		return false;
	}

	ret = recv(client.m_socket, buffer.data(), sizeof(Message::Header) + 8, 0);
	if (ret <= 0 or tempHeader->m_magic != magic or tempHeader->m_type != 0 or tempHeader->m_size != 8)
	{
		return false;
	}
	client.m_name = std::string(buffer.data() + sizeof(Message::Header), 8);
	std::cout << "[client: " << client.m_socket << "] -> [client: " << client.m_name << "]" << std::endl;

	return setTimeout(client, 0);
}

void listenToClient(Client& client, std::array<char, bufferSize>& buffer)
{
	size_t index = 0;
	size_t invalidBytesCount = 0;

	while (true)
	{
		auto ret = recv(client.m_socket, buffer.data() + index, 11, 0);
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

					if (tempHeader->m_magic != magic)
					{
						--index;
						++bufferIndex;
						++invalidBytesCount;
					}
					else
					{
						if (invalidBytesCount)
						{
							std::cout << "[client: " << client.m_name << "] Header found after " << invalidBytesCount << " invalid bytes" << std::endl;
							invalidBytesCount = 0;
						}
						header = tempHeader;
						break;
					}
				}

				if (header == nullptr or sizeof(Message::Header) + header->m_size > index)
				{
					if (bufferIndex > 0)
					{
						memcpy(buffer.data(), buffer.data() + bufferIndex, index);
					}
					break;
				}

				std::cout << "[client: " << client.m_name << "] ";

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
			std::cout << "[client: " << client.m_name << "] Closing connection" << std::endl;
			break;
		}
		else
		{
			std::cout << "[client: " << client.m_name << "] Failed to recieve message" << std::endl;
			break;
		}
	}
}

void handleClient(Client&& client)
{
	std::array<char, bufferSize> buffer;

	std::cout << "[client: " << client.m_socket << "] Opening connection" << std::endl;

	if (handshake(client, buffer))
	{
		listenToClient(client, buffer);
	}

	// shutdown the connection since we're done
	auto ret = shutdown(client.m_socket, SD_SEND);
	if (ret == SOCKET_ERROR)
	{
		std::cout << "[client: " << client.m_name << "] Shutdown failed with error: " << WSAGetLastError() << std::endl;
	}

	// cleanup
	closesocket(client.m_socket);
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
		SOCKET clientSocket = INVALID_SOCKET;
		clientSocket = accept(listenSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET)
		{
			std::cout << "accept failed with error: " << WSAGetLastError() << std::endl;
			continue;
		}

		Client client;
		client.m_socket = clientSocket;

		std::thread clientThread(handleClient, std::move(client));
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