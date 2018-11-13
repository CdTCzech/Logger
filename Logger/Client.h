#pragma once

#include <string>

#include <WinSock2.h>


struct Client
{
	SOCKET m_socket;
	std::string m_name;
};