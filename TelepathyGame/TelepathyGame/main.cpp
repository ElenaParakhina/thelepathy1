#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <iostream>
#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

// ���� ��������� ����� �������� � ��������
const int COMMAND_GAME_STARTED = 1; // ������ ����
const int COMMAND_GAME_FINISHED = 2; // ���� ���������
const int COMMAND_GAME_ABORTED = 3; // ���� �������� (�� ������������)
const int COMMAND_CARD_PICKED = 4; // ����� �������
const int COMMAND_HASH = 5; // ��������� ���
const int COMMAND_RANDOMED = 6; // ��������� ��������� �����, �� �������� �������� ��� ��������
const int COMMAND_CLIENT_RIGHT = 7; // ������ ������
const int COMMAND_CLIENT_WRONG = 8; // ���

const int N = 10; // ���-�� ���� � ����
int clientScore = 0; // ���� �������
std::vector<int> cardsGone; // ������ ��� ��������� �������� ���� (���� �� ����)

int computeHash(int n) // ������������ ��� ������ ����� � ���� ����� ��� ����
{
	int result = 0;
	while (n > 0)
	{
		result += n % 10;
		n /= 10;
	}

	return result;
}

int associateRandomNumber(int n) // ������ � ������������ ������ ����� n �� 0 �� N ��������� ����� �����, ������� n
{
	return n * (rand() % (RAND_MAX / N));
}

int __cdecl main(void)
{
	setlocale(LC_ALL, "rus");
	srand(time(0)); // ������������� ���������� ��������������� �����
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET; // �����, ������� ������� ������ �� ������� ����������
	SOCKET ClientSocket = INVALID_SOCKET; // ����� ��� �������� ������ � ��������

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// ��������� ����� � ���� �������
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);//������� ��������� ������ ����������� � �������
		WSACleanup();
		return 1;
	}

	// �������� ������ ��� ����������� � �������
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());//������ ������
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// ��������� TCP ���������� ������
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());//��������� ���� ���������� � �������
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	// ������������� ������, ��� ������ (�������)
	printf("�������� �������...\n");

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// ����� �������������
	// ��������� ����� �������
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());//����� ������� ���������� � �������
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// ������ ������� �� ���, ��������� ����� ��� �������������
	closesocket(ListenSocket);

	printf("������ �����������\n");
	
	// ���� ��������
	int currentCommandCode = COMMAND_GAME_STARTED; // ��� ������������ ������� (���� ��������)
	iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0); // ������� ������� �������
	if (iSendResult == SOCKET_ERROR) { // ���� �� �������, ��������� ������, ������� ������ (��� � �����)
		printf("send failed with error: %d\n", WSAGetLastError()); //���� ��������
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}
	
	int receivedCommand = 0; // ���������� �� ������� �������
	iResult = recv(ClientSocket, (char*)&receivedCommand, sizeof(int), 0);
	if (iResult > 0) // ���� �������� ������
	{
		if (receivedCommand == COMMAND_GAME_STARTED) // ���� ��� ������� ���� ��������, �� ������� ��������� �� ���� � ��� �����
			printf("���� ��������\n");
	}
	else if (iResult == 0)
	{
		printf("Connection closed");//���������� �������
		return 0;
	}
	else
	{
		printf("recv failed with error: %d\n", WSAGetLastError()); //������� ����������� � �������
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	int randomNumber; // ��������� �����, ������� �� ����������� � ��������� ������, ����� �� ���� ��������� ��� � �������� ������� (�����)

	while (cardsGone.size() != N - 1) // ���� �� ������� N - 1 ����
	{
		int pickedCard = -1; // ��������� �����
		do // � ����� ������ ������ ����� �����, ���� �� ����� ����� ����������
		{
			printf("���������� ������� ����� ��: ");
			for (int i = 0; i < N; i++) // ��������� ��������� �����. ������� ��� �� ������
				if (std::find(cardsGone.begin(), cardsGone.end(), i) == cardsGone.end()) // ��� �� ������ ���� � ������� �������� ����
				{
					printf("%d", i);
					if (i != N - 1) // ���� ����� �� ��������, �� ������ ������� � ������
						printf(", ");
				}
			printf(".\n"); // ����� ��������
			scanf_s("%d", &pickedCard); // ��������� ����� ���������� �����
		} while (pickedCard < 0 || pickedCard >= N || std::find(cardsGone.begin(), cardsGone.end(), pickedCard) != cardsGone.end()); // ���� �� ����� ����� ���������� ����� (�� 0 �� N-1, ����� �� ��� � ����)

		cardsGone.push_back(pickedCard); // ����� ���� ������������. ��������
		currentCommandCode = COMMAND_CARD_PICKED; // �������� ������� � ������ �����
		iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
		/*������� ��������� �����, ������� ������ �����. 
		������������ �� ���������� ����� ��� � ���������� �������. 
		������ ��������� �����, ������ ���������� ��������� �����.
		���� ��� ������ ����� � ��� ����������� �������� ��������� � ����� �������, ������ �� ����*/
		randomNumber = associateRandomNumber(pickedCard); // ������� ��������� �����, ��������������� � ������� �����
		int hash = computeHash(randomNumber); // ���������� ��� �� ����
		currentCommandCode = COMMAND_HASH; // � �������� ������� ��������� ��������� ��� ���
		iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
		// � ������ � ��� ���
		iSendResult = send(ClientSocket, (char*)&hash, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

		// ��� ����� �������
		printf("�������� ������� �������...\n");

		receivedCommand = 0;
		// ������ ���-�� ������
		iResult = recv(ClientSocket, (char*)&receivedCommand, sizeof(int), 0);
		if (iResult > 0)
		{
			if (receivedCommand == COMMAND_CARD_PICKED) // ���� ������ ������� ����� �������
			{
				int guessCard;
				iResult = recv(ClientSocket, (char*)&guessCard, sizeof(int), 0); // ��������� ����� �����
				if (iResult > 0)
				{
						printf("������ �������, ��� ��� %d\n", guessCard);
						if (guessCard == pickedCard) // ���� ����� ������� � ������� ������
						{
							currentCommandCode = COMMAND_CLIENT_RIGHT; // ��������� ������� ������� ����� �� ����
							clientScore += N - cardsGone.size() + 1; // ���������� ���� �������
						}
						else
							currentCommandCode = COMMAND_CLIENT_WRONG; // ����� �� ������


						// �������� ��������� �������� ������� �������
					iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						WSACleanup();
						return 1;
					}

					// ���� ���������, ��� ������ �� ����, �� �������� ����� ���������� �����
					if (currentCommandCode == COMMAND_CLIENT_WRONG)
					{
						iSendResult = send(ClientSocket, (char*)&pickedCard, sizeof(int), 0);
						if (iSendResult == SOCKET_ERROR) {
							printf("send failed with error: %d\n", WSAGetLastError());
							closesocket(ClientSocket);
							WSACleanup();
							return 1;
						}
					}

					// ������ ���������� � ����� ������ ��������� �����, �� �������� ������� ���
					// ������� ���������, ��� ��� ��� �����
					currentCommandCode = COMMAND_RANDOMED;
					iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						WSACleanup();
						return 1;
					}
					// ����� ���� �����
					iSendResult = send(ClientSocket, (char*)&randomNumber, sizeof(int), 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						WSACleanup();
						return 1;
					}

					// ������� ���� �������
					printf("���� �������: %d\n", clientScore);
				}
				else if (iResult == 0)
				{
					printf("Connection closed");
					return 0;
				}
				else
				{
					printf("recv failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					return 1;
				}
			}
		}
		else if (iResult == 0)
		{
			printf("Connection closed");
			return 0;
		}
		else
		{
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
		
		// � ��� � �����
	}

	// ���� ����������, ���� ��������, ������� �������
	currentCommandCode = COMMAND_GAME_FINISHED;
	iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}
	printf("���� ��������\n");




	// shutdown the connection since we're done
	//���������� ����������
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	//�������
	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}
