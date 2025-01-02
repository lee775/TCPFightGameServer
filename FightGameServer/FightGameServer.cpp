#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Winmm.lib")
#include <WinSock2.h>
#include <iostream>
#include <Windows.h>
#include <ws2tcpip.h>
#include <list>
#include "PacketDefine.h"
#include "RingBuffer.h"
#include "SerialBuffer.h"
#include <unordered_map>

#define SERVERPORT 5000
#define SPEEDX 3
#define SPEEDY 2

using namespace std;

int id = 1;

struct Player
{
	SOCKET _sock;
	DWORD _id;
	WCHAR _ip[16];
	WORD _port;
	RingBuffer _rRing;
	RingBuffer _sRing;

	char _action;
	BYTE _direction;

	WORD _x;
	WORD _y;
	char _hp;


	Player() : _hp(100), _id(id++), _action(-1) {}
};
list<Player*> playerList;

//#include <functional>
//using PacketHandler = std::function<void(const char*, BYTE)>;
typedef void (*PacketHandler)(Player* player, char* data);

void Initialize();
void netIOProcess();
void Update();

int AcceptProc(SOCKET listen_sock);
int RecvProc(Player* player);
int RingBufSend(Player* player);

//일반 구조체 사용 형식
int SendUnicast(Player* sendPlayer, char* header, int headerSize, char* data);
int SendBroadcast(Player* notSendPlayer, char* header, int headerSize, char* data);
int SendAll(char* header, int headerSize, char* data);

//직렬화 버퍼 사용 형식
int SendUnicast(Player* sendPlayer, Packet* packet);
int SendBroadcast(Player* notSendPlayer, Packet* packet);
int SendAll(Packet* packet);

int Disconnect(Player* player);

void MoveStart(Player* player, char* data);
void MoveStop(Player* player, char* data);
void Attack1(Player* player, char* data);
void Attack2(Player* player, char* data);
void Attack3(Player* player, char* data);

void MovePlayer(Player* player);

void Attack(Player* attacker, BYTE damage, int rangeX, int rangeY);
bool CheckAttackCollision(Player* attacker, Player* target, int rangeX, int rangeY);

void mpHeader(Header* header, BYTE size, BYTE type);
void mpCreateMyChar(Header* header, SC_CreateMyCharacter* packet, DWORD id, BYTE dir, WORD x, WORD y, BYTE hp);
void mpCreateOtherChar(Header* header, SC_CreateOtherCharacter* packet, DWORD id, BYTE dir, WORD x, WORD y, BYTE hp);
void mpDeleteChar(Header* header, SC_DeleteCharacter* packet, DWORD id);
void mpMoveStart(Header* header, SC_MoveStart* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpMoveStop(Header* header, SC_MoveStop* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpAttack1(Header* header, SC_Attack1* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpAttack2(Header* header, SC_Attack2* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpAttack3(Header* header, SC_Attack3* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpDamage(Header* header, SC_Damage* packet, DWORD attackId, DWORD damageId, BYTE damageHp);

void mpHeader(Packet* packet, BYTE size, BYTE type);
void mpCreateMyChar(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y, BYTE hp);
void mpCreateOtherChar(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y, BYTE hp);
void mpDeleteChar(Packet* packet, DWORD id);
void mpMoveStart(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpMoveStop(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpAttack1(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpAttack2(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpAttack3(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y);
void mpDamage(Packet* packet, DWORD attackId, DWORD damageId, BYTE damageHp);

std::unordered_map<BYTE, PacketHandler> dispatchTable = {
	{dfPACKET_CS_MOVE_START, MoveStart},
	{dfPACKET_CS_MOVE_STOP, MoveStop},
	{dfPACKET_CS_ATTACK1, Attack1},
	{dfPACKET_CS_ATTACK2, Attack2},
	{dfPACKET_CS_ATTACK3, Attack3},
};

WSADATA wsa;
SOCKET listen_sock;

DWORD startTime;
DWORD sleepTime = -1;
int wmain()
{
	srand(time(NULL));
	timeBeginPeriod(1);

	Initialize();

	startTime = timeGetTime();
	while (1)
	{
		netIOProcess();

		Update();
	}
	//리슨 소켓 종료
	closesocket(listen_sock);

	//윈속 종료
	WSACleanup();
}

void Initialize()
{
	int ret;

	//윈속 초기화
	wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return;

	//리슨 소켓 생성
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
	{
		wprintf(L"listen_sock socket() err : %d", WSAGetLastError());
		return;
	}

	linger ling;
	ling.l_onoff = 1;
	ling.l_linger = 0;
	setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));

	//바인딩
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	ret = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (ret == SOCKET_ERROR)
	{
		wprintf(L"bind() err : %d", WSAGetLastError());
		return;
	}

	//리슨
	ret = listen(listen_sock, SOMAXCONN);
	if (ret == SOCKET_ERROR)
	{
		wprintf(L"listen() err : %d", WSAGetLastError());
		return;
	}

	//넌블로킹소켓으로 전환
	u_long on = 1;
	ret = ioctlsocket(listen_sock, FIONBIO, &on);
	if (ret == SOCKET_ERROR)
	{
		wprintf(L"ioctlsocket() err : %d", WSAGetLastError());
		return;
	}
}

void netIOProcess()
{
	int ret;

	FD_SET rset;
	FD_SET sset;
	FD_ZERO(&rset);
	FD_ZERO(&sset);
	FD_SET(listen_sock, &rset);

	list<Player*>::iterator iter;
	for (iter = playerList.begin(); iter != playerList.end();++iter)
	{
		if ((*iter)->_hp > 0)
		{
			FD_SET((*iter)->_sock, &rset);
			if ((*iter)->_sRing.GetUseSize() > 0)
				FD_SET((*iter)->_sock, &sset);
		}
	}

	timeval time = { 0, 0 };
	ret = select(0, &rset, &sset, NULL, &time);
	if (FD_ISSET(listen_sock, &rset))
	{
		int acceptRet = AcceptProc(listen_sock);
		//앞에서 부터 RST / 논블로킹소켓 작업 아직안끝남 / 4way-handshake / 문제없음 네가지 경우가 아닐때
		if (acceptRet != WSAECONNRESET && acceptRet != WSAEWOULDBLOCK && acceptRet != -1 && acceptRet != 0)
		{
			wprintf(L"error : %d", acceptRet);
			//문제 소켓은 다음프레임에 삭제처리 예정
			return;
		}
	}

	for (iter = playerList.begin(); iter != playerList.end(); ++iter)
	{
		if ((*iter)->_hp > 0)
		{
			if (FD_ISSET((*iter)->_sock, &rset))
			{
				int recvRet = RecvProc((*iter));
				if (recvRet != WSAECONNRESET && recvRet != WSAEWOULDBLOCK && recvRet != -1 && recvRet != 0)
				{
					printf("recv error : %d", recvRet);
					return;
				}
			}
			if (FD_ISSET((*iter)->_sock, &sset))
			{
				int sendRet = RingBufSend((*iter));
				if (sendRet != WSAECONNRESET && sendRet != WSAEWOULDBLOCK && sendRet != -1 && sendRet != 0)
				{
					printf("send error : %d", sendRet);
					return;
				}
			}
		}
	}
}

void Update()
{
	//프레임 맞추기 추후 시간으로 변경 해볼 예정
	sleepTime = timeGetTime() - startTime;
	if (sleepTime >= 20)
	{
		list<Player*>::iterator iter;
		for (iter = playerList.begin(); iter != playerList.end();)
		{
			if ((*iter)->_hp > 0)
			{
				if ((*iter)->_action != -1)
					MovePlayer(*iter);
				++iter;
			}
			else
			{
				Disconnect((*iter));
				delete (*iter);
				iter = playerList.erase(iter);
			}
		}
		//if (sleepTime < 20)
		//	Sleep(20 - sleepTime);
		startTime += 20;
	}
}

int AcceptProc(SOCKET listen_sock)
{
	int ret;

	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;

	addrlen = sizeof(clientaddr);
	client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
	if (client_sock == INVALID_SOCKET)
	{
		wprintf(L"accept() err : %d", WSAGetLastError());
		return WSAGetLastError();
	}

	Player* newPlayer = new Player;
	InetNtopW(AF_INET, &clientaddr.sin_addr, newPlayer->_ip, INET_ADDRSTRLEN);
	newPlayer->_port = ntohs(clientaddr.sin_port);
	wprintf(L"\n[TCP Server] Client Connect : IP addr = %s, port = %d\n", newPlayer->_ip, newPlayer->_port);
	newPlayer->_direction = dfPACKET_MOVE_DIR_LL;
	newPlayer->_x = ((rand() % 204) * 3) + 12;
	newPlayer->_y = ((rand() % 200) * 2) + 50;
	newPlayer->_sock = client_sock;

	wprintf(L"[Position] X : %d, Y : %d / %d\n", newPlayer->_x, newPlayer->_y, sizeof(SC_CreateMyCharacter));

	//Header createMyCharHeader;
	//SC_CreateMyCharacter createMyChar;
	//mpCreateMyChar(&createMyCharHeader, &createMyChar, newPlayer->_id, newPlayer->_direction, newPlayer->_x, newPlayer->_y, 100);

	////id 전송
	//ret = SendUnicast(newPlayer, (char*)&createMyCharHeader, sizeof(Header), (char*)&createMyChar);
	//if (ret != 0)
	//{
	//	return ret;
	//}

	Packet packet;
	mpCreateMyChar(&packet, newPlayer->_id, newPlayer->_direction, newPlayer->_x, newPlayer->_y, 100);
	 
	printf("a : %d\n",packet.GetDataSize());
	//id 전송
	ret = SendUnicast(newPlayer, &packet);
	if (ret != 0)
	{
		return ret;
	}

	Header createOtherCharHeader;
	SC_CreateOtherCharacter createOtherChar;
	mpCreateOtherChar(&createOtherCharHeader, &createOtherChar, newPlayer->_id, newPlayer->_direction, newPlayer->_x, newPlayer->_y, 100);

	//다른유저에게 신규유저 생성좌표 전송
	ret = SendBroadcast(newPlayer, (char*)&createOtherCharHeader, sizeof(Header), (char*)&createOtherChar);
	if (ret != 0)
	{
		return ret;
	}

	//신규유저에게 기존 유저 정보 전송
	list<Player*>::iterator iter;
	for (iter = playerList.begin(); iter != playerList.end(); ++iter)
	{
		Header newPlayerCreateOtherCharHeader;
		SC_CreateOtherCharacter newPlayerCreateOtherChar;
		mpCreateOtherChar(&newPlayerCreateOtherCharHeader, &newPlayerCreateOtherChar, (*iter)->_id, (*iter)->_direction, (*iter)->_x, (*iter)->_y, (*iter)->_hp);

		ret = SendUnicast(newPlayer, (char*)&newPlayerCreateOtherCharHeader, sizeof(Header), (char*)&newPlayerCreateOtherChar);
		if (ret != 0)
		{
			return ret;
		}
		//기존 유저가 이동중에 신규유저가 들어오면 이동패킷도 같이 보내줌
		if ((*iter)->_action != -1)
		{
			Header otherPlayerMoveStartHeader;
			SC_MoveStart otherPlayerMoveStart;
			mpMoveStart(&otherPlayerMoveStartHeader, &otherPlayerMoveStart, (*iter)->_id, (*iter)->_action, (*iter)->_x, (*iter)->_y);
			ret = SendUnicast(newPlayer, (char*)&otherPlayerMoveStartHeader, sizeof(Header), (char*)&otherPlayerMoveStart);
		}
	}

	//신규유저 정보를 list에 저장
	playerList.push_back(newPlayer);

	return 0;
}

int RecvProc(Player* player)
{
	int ret;
	char packetBuf[100];

	//바로 넣을 수 있으면 바로 넣어주고 아니면 버퍼를 만들어서 넣어줌
	if (player->_rRing.DirectEnqueueSize() >= player->_rRing.GetFreeSize())
	{
		ret = recv(player->_sock, player->_rRing.GetWriteBufferPtr(), player->_rRing.DirectEnqueueSize(), 0);
		if (ret == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				wprintf(L"recv err() : %d\n", WSAGetLastError());
				player->_hp = 0;
				return -1;
			}
		}
		else if (ret == 0)
		{
			player->_hp = 0;
			return -1;
		}
		player->_rRing.MoveWrite(ret);
	}
	else
	{
		int size = player->_rRing.GetFreeSize();
		char* buf = new char[size];

		ret = recv(player->_sock, buf, size, 0);
		if (ret == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				wprintf(L"recv err() : %d\n", WSAGetLastError());
				player->_hp = 0;
				delete[] buf;
				return -1;
			}
		}
		else if (ret == 0)
		{
			player->_hp = 0;
			return -1;
		}
		player->_rRing.Enqueue(buf, ret);
		delete[] buf;
	}

	while (1)
	{
		if (player->_rRing.GetUseSize() <= sizeof(Header))
			break;

		Header header;
		player->_rRing.Peek((char*)&header, sizeof(Header));
		if (header._code != 0x89)
		{
			//player->_rRing.MoveRead(sizeof(Header) + header._size);
			player->_hp = 0;
			return 0;
		}

		if (player->_rRing.GetUseSize() < sizeof(Header) + header._size)
			break;
		player->_rRing.MoveRead(sizeof(Header));
		player->_rRing.Peek(packetBuf, header._size);

		//각 패킷메세지는 map형태로 함수와 매핑되어있음
		std::unordered_map<BYTE, PacketHandler>::iterator iter = dispatchTable.find(header._type);
		if (iter != dispatchTable.end())
			iter->second(player, packetBuf);
		else
		{
			wprintf(L"The packet message is invalid.");
		}
		player->_rRing.MoveRead(header._size);
	}
	return 0;
}

int SendUnicast(Player* sendPlayer, char* header, int headerSize, char* data)
{
	//링버퍼 삽입으로 변경
	if (!sendPlayer->_sRing.Enqueue(header, headerSize))
	{
		wprintf(L"Header Enqueue Fail");
		return -1;
	}
	if (!sendPlayer->_sRing.Enqueue(data, ((Header*)header)->_size))
	{
		wprintf(L"data Enqueue Fail");
		return -1;
	}

	return 0;
}

int SendBroadcast(Player* notSendPlayer, char* header, int headerSize, char* data)
{
	int ret;
	list<Player*>::iterator iter;
	for (iter = playerList.begin(); iter != playerList.end(); ++iter)
	{
		if ((*iter)->_id != notSendPlayer->_id)
		{
			ret = SendUnicast((*iter), header, headerSize, data);
			if (ret != 0 && ret != -1 && ret != WSAECONNRESET)
			{
				return ret;
			}
		}
	}
	return 0;
}

int SendAll(char* header, int headerSize, char* data)
{
	int ret;
	list<Player*>::iterator iter;
	for (iter = playerList.begin(); iter != playerList.end(); ++iter)
	{
		ret = SendUnicast((*iter), header, headerSize, data);
		if (ret != 0 && ret != -1 && ret != WSAECONNRESET)
		{
			return ret;
		}
	}
	return 0;
}

int SendUnicast(Player* sendPlayer, Packet* packet)
{
	//링버퍼에 직접 삽입
	int ret = packet->GetBuffer(sendPlayer->_sRing.GetWriteBufferPtr(), sendPlayer->_sRing.DirectEnqueueSize());
	if (ret == sendPlayer->_sRing.DirectEnqueueSize())
	{
		//다 안들어갓으면 마저 넣어줌
		sendPlayer->_sRing.MoveWrite(ret);
		packet->GetBuffer(sendPlayer->_sRing.GetWriteBufferPtr(), sendPlayer->_sRing.DirectEnqueueSize());
	}
	else
	{
		sendPlayer->_sRing.MoveWrite(ret);
	}
	return 0;
}

int SendBroadcast(Player* notSendPlayer, Packet* packet)
{
	int ret;
	list<Player*>::iterator iter;
	for (iter = playerList.begin(); iter != playerList.end(); ++iter)
	{
		if ((*iter)->_id != notSendPlayer->_id)
		{
			ret = SendUnicast((*iter), packet);
			if (ret != 0 && ret != -1 && ret != WSAECONNRESET)
			{
				return ret;
			}
		}
	}
	return 0;
}

int SendAll(Packet* packet)
{
	int ret;
	list<Player*>::iterator iter;
	for (iter = playerList.begin(); iter != playerList.end(); ++iter)
	{
		ret = SendUnicast((*iter), packet);
		if (ret != 0 && ret != -1 && ret != WSAECONNRESET)
		{
			return ret;
		}
	}
	return 0;
}

int RingBufSend(Player* player)
{
	int ret;
	//전부 한번에 보낼 수 있으면 보내고 없으면 버퍼에 담아서 보냄
	if (player->_sRing.DirectDequeueSize() >= player->_sRing.GetUseSize())
	{
		ret = send(player->_sock, player->_sRing.GetReadBufferPtr(), player->_sRing.GetUseSize(), 0);
		if (ret == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				wprintf(L"send err() : %d", WSAGetLastError());
				return -1;
			}
		}
		//wprintf(L"send : %d\n", ret);
		player->_sRing.MoveRead(ret);
	}
	else
	{
		int size = player->_sRing.GetUseSize();
		char* buf = new char[size];
		if (!player->_sRing.Peek(buf, size))
		{
			wprintf(L"send Peek err()");
			return -1;
		}
		ret = send(player->_sock, buf, size, 0);
		if (ret == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				wprintf(L"send err() : %d", WSAGetLastError());
				return -1;
			}
		}
		player->_sRing.MoveRead(ret);
	}
	return 0;
}

int Disconnect(Player* player)
{
	Header header;
	header._code = 0x89;
	header._size = sizeof(SC_DeleteCharacter);
	header._type = dfPACKET_SC_DELETE_CHARACTER;

	SC_DeleteCharacter deleteChar;
	int ret;

	deleteChar._id = player->_id;

	ret = SendBroadcast(player, (char*)&header, sizeof(Header), (char*)&deleteChar);
	closesocket(player->_sock);
	if (ret != 0)
	{
		return ret;
	}
	return 0;
}

void MoveStart(Player* player, char* data)
{
	wprintf(L"moveStart\n");
	CS_MoveStart* cs_moveStart = (CS_MoveStart*)data;
	player->_action = cs_moveStart->_direction;

	switch (cs_moveStart->_direction)
	{
	case dfPACKET_MOVE_DIR_LL:
	case dfPACKET_MOVE_DIR_LU:
	case dfPACKET_MOVE_DIR_LD:
		player->_direction = dfPACKET_MOVE_DIR_LL;
		break;
	case dfPACKET_MOVE_DIR_RU:
	case dfPACKET_MOVE_DIR_RR:
	case dfPACKET_MOVE_DIR_RD:
		player->_direction = dfPACKET_MOVE_DIR_RR;
		break;
	}

	Header header;
	SC_MoveStart sc_moveStart;
	mpMoveStart(&header, &sc_moveStart, player->_id, player->_action, player->_x, player->_y);

	SendBroadcast(player, (char*)&header, sizeof(Header), (char*)&sc_moveStart);
}

void MoveStop(Player* player, char* data)
{
	CS_MoveStop* cs_moveStop = (CS_MoveStop*)data;

	int x = abs(player->_x - cs_moveStop->_x);
	int y = abs(player->_y - cs_moveStop->_y);
	if (x >= ERROR_RANGE || y >= ERROR_RANGE)
	{
		wprintf(L"s %d / %d \n", player->_x, player->_y);
		wprintf(L"c %d / %d \n", cs_moveStop->_x, cs_moveStop->_y);
		player->_hp = 0;
		return;
	}

	player->_action = -1;
	player->_direction = cs_moveStop->_direction;
	player->_x = cs_moveStop->_x;
	player->_y = cs_moveStop->_y;

	Header header;
	SC_MoveStop sc_moveStop;
	mpMoveStop(&header, &sc_moveStop, player->_id, player->_direction, player->_x, player->_y);

	SendBroadcast(player, (char*)&header, sizeof(Header), (char*)&sc_moveStop);
}

void Attack1(Player* player, char* data)
{
	//CS_Attack1* cs_attack = (CS_Attack1*)data;
	Header header;
	SC_Attack1 sc_attack1;
	mpAttack1(&header, &sc_attack1, player->_id, player->_direction, player->_x, player->_y);
	SendAll((char*)&header, sizeof(Header), (char*)&sc_attack1);
	Attack(player, ATTACK1_DAMAGE, dfATTACK1_RANGE_X, dfATTACK1_RANGE_Y);
}

void Attack2(Player* player, char* data)
{
	//CS_Attack2* cs_attack = (CS_Attack2*)data;
	Header header;
	SC_Attack2 sc_attack2;
	mpAttack2(&header, &sc_attack2, player->_id, player->_direction, player->_x, player->_y);
	SendAll((char*)&header, sizeof(Header), (char*)&sc_attack2);
	Attack(player, ATTACK2_DAMAGE, dfATTACK2_RANGE_X, dfATTACK2_RANGE_Y);
}

void Attack3(Player* player, char* data)
{
	//CS_Attack3* cs_attack = (CS_Attack3*)data;
	Header header;
	SC_Attack3 sc_attack3;
	mpAttack3(&header, &sc_attack3, player->_id, player->_direction, player->_x, player->_y);
	SendAll((char*)&header, sizeof(Header), (char*)&sc_attack3);
	Attack(player, ATTACK3_DAMAGE, dfATTACK3_RANGE_X, dfATTACK3_RANGE_Y);
}

void Attack(Player* attacker, BYTE damage, int rangeX, int rangeY)
{
	list<Player*>::iterator iter;
	for (iter = playerList.begin(); iter != playerList.end(); ++iter)
	{
		if ((*iter)->_id != attacker->_id)
		{
			if (CheckAttackCollision(attacker, *iter, rangeX, rangeY))
			{
				(*iter)->_hp -= damage;
				wprintf(L"hp : %d", (*iter)->_hp);
				Header header;
				SC_Damage sc_damage;
				mpDamage(&header, &sc_damage, attacker->_id, (*iter)->_id, (*iter)->_hp);
				SendAll((char*)&header, sizeof(Header), (char*)&sc_damage);
			}
		}
	}
}

bool CheckAttackCollision(Player* attacker, Player* target, int rangeX, int rangeY)
{
	switch (attacker->_direction)
	{
	case dfPACKET_MOVE_DIR_LL:
	{
		int x = attacker->_x - rangeX;
		int y = attacker->_y - (rangeY / 2);
		if (target->_x >= x && target->_x <= attacker->_x && target->_y >= y && target->_y <= attacker->_y + (rangeY / 2))
			return true;
		break;
	}
	case dfPACKET_MOVE_DIR_RR:
	{
		int x = attacker->_x + rangeX;
		int y = attacker->_y + (rangeY / 2);
		if (target->_x <= x && target->_x >= attacker->_x && target->_y <= y && target->_y >= attacker->_y - (rangeY / 2))
			return true;
		break;
	}
	}
	return false;
}

void MovePlayer(Player* player)
{
	int oldX = player->_x;
	int oldY = player->_y;

	switch (player->_action)
	{
	case dfPACKET_MOVE_DIR_LL:
	{
		player->_x -= SPEEDX;
	}
	break;
	case dfPACKET_MOVE_DIR_LU:
	{
		player->_x -= SPEEDX;
		player->_y -= SPEEDY;
	}
	break;
	case dfPACKET_MOVE_DIR_UU:
	{
		player->_y -= SPEEDY;
	}
	break;
	case dfPACKET_MOVE_DIR_RU:
	{
		player->_x += SPEEDX;
		player->_y -= SPEEDY;
	}
	break;
	case dfPACKET_MOVE_DIR_RR:
	{
		player->_x += SPEEDX;
	}
	break;
	case dfPACKET_MOVE_DIR_RD:
	{
		player->_x += SPEEDX;
		player->_y += SPEEDY;
	}
	break;
	case dfPACKET_MOVE_DIR_DD:
	{
		player->_y += SPEEDY;
	}
	break;
	case dfPACKET_MOVE_DIR_LD:
	{
		player->_x -= SPEEDX;
		player->_y += SPEEDY;
	}
	break;
	}

	if (player->_x < RANGE_MOVE_LEFT)
	{
		player->_x = RANGE_MOVE_LEFT;
		player->_y = oldY;
	}
	else if (player->_x > RANGE_MOVE_RIGHT)
	{
		player->_x = RANGE_MOVE_RIGHT;
		player->_y = oldY;
	}
	else if (player->_y < RANGE_MOVE_TOP)
	{
		player->_x = oldX;
		player->_y = RANGE_MOVE_TOP;
	}
	else if (player->_y > RANGE_MOVE_BOTTOM)
	{
		player->_x = oldX;
		player->_y = RANGE_MOVE_BOTTOM;
	}

	wprintf(L"move, X : %d, Y : %d\n", player->_x, player->_y);
}

void mpHeader(Header* header, BYTE size, BYTE type)
{
	header->_code = 0x89;
	header->_size = size;
	header->_type = type;
}

// dfPACKET_SC_CREATE_MY_CHARACTER
void mpCreateMyChar(Header* header, SC_CreateMyCharacter* packet, DWORD id, BYTE dir, WORD x, WORD y, BYTE hp)
{
	mpHeader(header, sizeof(SC_CreateMyCharacter), dfPACKET_SC_CREATE_MY_CHARACTER);

	packet->_id = id;
	packet->_direction = dir;
	packet->_x = x;
	packet->_y = y;
	packet->_hp = hp;
}

// dfPACKET_SC_CREATE_OTHER_CHARACTER
void mpCreateOtherChar(Header* header, SC_CreateOtherCharacter* packet, DWORD id, BYTE dir, WORD x, WORD y, BYTE hp)
{
	mpHeader(header, sizeof(SC_CreateOtherCharacter), dfPACKET_SC_CREATE_OTHER_CHARACTER);

	packet->_id = id;
	packet->_direction = dir;
	packet->_x = x;
	packet->_y = y;
	packet->_hp = hp;
}

//dfPACKET_SC_DELETE_CHARACTER
void mpDeleteChar(Header* header, SC_DeleteCharacter* packet, DWORD id)
{
	mpHeader(header, sizeof(SC_DeleteCharacter), dfPACKET_SC_DELETE_CHARACTER);

	packet->_id = id;
}

//dfPACKET_SC_MOVE_START
void mpMoveStart(Header* header, SC_MoveStart* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(header, sizeof(SC_MoveStart), dfPACKET_SC_MOVE_START);

	packet->_direction = dir;
	packet->_id = id;
	packet->_x = x;
	packet->_y = y;
}

//dfPACKET_SC_MOVE_STOP
void mpMoveStop(Header* header, SC_MoveStop* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(header, sizeof(SC_MoveStop), dfPACKET_SC_MOVE_STOP);

	packet->_id = id;
	packet->_direction = dir;
	packet->_x = x;
	packet->_y = y;
}

//dfPACKET_SC_ATTACK1
void mpAttack1(Header* header, SC_Attack1* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(header, sizeof(SC_Attack1), dfPACKET_SC_ATTACK1);

	packet->_id = id;
	packet->_direction = dir;
	packet->_x = x;
	packet->_y = y;
}

//dfPACKET_SC_ATTACK2
void mpAttack2(Header* header, SC_Attack2* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(header, sizeof(SC_Attack2), dfPACKET_SC_ATTACK2);

	packet->_id = id;
	packet->_direction = dir;
	packet->_x = x;
	packet->_y = y;
}

//dfPACKET_SC_ATTACK3
void mpAttack3(Header* header, SC_Attack3* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(header, sizeof(SC_Attack3), dfPACKET_SC_ATTACK3);

	packet->_id = id;
	packet->_direction = dir;
	packet->_x = x;
	packet->_y = y;
}

//dfPACKET_SC_DAMAGE
void mpDamage(Header* header, SC_Damage* packet, DWORD attackId, DWORD damageId, BYTE damageHp)
{
	mpHeader(header, sizeof(SC_Damage), dfPACKET_SC_DAMAGE);

	packet->_attackId = attackId;
	packet->_damageId = damageId;
	packet->_damageHp = damageHp;
}

void mpHeader(Packet* packet, BYTE size, BYTE type)
{
	*packet << (BYTE)0x89 << size << type;
}

void mpCreateMyChar(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y, BYTE hp)
{
	mpHeader(packet, sizeof(SC_CreateMyCharacter), dfPACKET_SC_CREATE_MY_CHARACTER);

	*packet << id << dir << x << y << hp;
}

void mpCreateOtherChar(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y, BYTE hp)
{
	mpHeader(packet, sizeof(SC_CreateOtherCharacter), dfPACKET_SC_CREATE_OTHER_CHARACTER);

	*packet << id << dir << x << y << hp;
}

void mpDeleteChar(Packet* packet, DWORD id)
{
	mpHeader(packet, sizeof(SC_DeleteCharacter), dfPACKET_SC_DELETE_CHARACTER);

	*packet << id;
}

void mpMoveStart(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(packet, sizeof(SC_MoveStart), dfPACKET_SC_MOVE_START);

	*packet << id << dir << x << y;
}

void mpMoveStop(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(packet, sizeof(SC_MoveStop), dfPACKET_SC_MOVE_STOP);

	*packet << id << dir << x << y;
}

void mpAttack1(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(packet, sizeof(SC_Attack1), dfPACKET_SC_ATTACK1);

	*packet << id << dir << x << y;
}

void mpAttack2(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(packet, sizeof(SC_Attack2), dfPACKET_SC_ATTACK2);

	*packet << id << dir << x << y;
}

void mpAttack3(Packet* packet, DWORD id, BYTE dir, WORD x, WORD y)
{
	mpHeader(packet, sizeof(SC_Attack3), dfPACKET_SC_ATTACK3);

	*packet << id << dir << x << y;
}

void mpDamage(Packet* packet, DWORD attackId, DWORD damageId, BYTE damageHp)
{
	mpHeader(packet, sizeof(SC_Damage), dfPACKET_SC_DAMAGE);

	*packet << attackId << damageId << damageHp;
}
