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

// Коды сообщений между сервером и клиентом
const int COMMAND_GAME_STARTED = 1; // Начало игры
const int COMMAND_GAME_FINISHED = 2; // Игра закончена
const int COMMAND_GAME_ABORTED = 3; // Игра отменена (не используется)
const int COMMAND_CARD_PICKED = 4; // Карта выбрана
const int COMMAND_HASH = 5; // Передаётся хэш
const int COMMAND_RANDOMED = 6; // Передаётся случайное число, от которого считался хэш сервером
const int COMMAND_CLIENT_RIGHT = 7; // Клиент угадал
const int COMMAND_CLIENT_WRONG = 8; // Нет

const int N = 10; // Кол-во карт в игре
int clientScore = 0; // счёт клиента
std::vector<int> cardsGone; // вектор уже выбранных сервером карт (ушли из игры)

int computeHash(int n) // рассчитывает хэш целого числа в виде суммы его цифр
{
	int result = 0;
	while (n > 0)
	{
		result += n % 10;
		n /= 10;
	}

	return result;
}

int associateRandomNumber(int n) // ставит в соответствие целому числу n от 0 до N случайное целое число, кратное n
{
	return n * (rand() % (RAND_MAX / N));
}

int __cdecl main(void)
{
	setlocale(LC_ALL, "rus");
	srand(time(0)); // Инициализация генератора псевдослучайных чисел
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET; // сокет, который случает сервер на предмет соединений
	SOCKET ClientSocket = INVALID_SOCKET; // сокет для передачи данных с клиентом

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

	// Установка адрес и порт сервера
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);//функция получения адреса завершилась с ошибкой
		WSACleanup();
		return 1;
	}

	// Создайте сокета для подключения к серверу
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());//ошибка сокета
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Установка TCP соединения сокета
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());//произошел сбой соединения с ошибкой
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	// прослушивание начато, ждём игрока (клиента)
	printf("ожидание клиента...\n");

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// игрок присоединился
	// Принимаем сокет клиента
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());//прием клиента завершился с ошибкой
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// других игроков не ждём, закрывает сокет для прослушивания
	closesocket(ListenSocket);

	printf("клиент подключился\n");
	
	// игра началась
	int currentCommandCode = COMMAND_GAME_STARTED; // код передаваемой команды (игра началась)
	iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0); // передаём команду клиенту
	if (iSendResult == SOCKET_ERROR) { // если не удалось, завершаем работу, выводим ошибку (как и везде)
		printf("send failed with error: %d\n", WSAGetLastError()); //сбой передачи
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}
	
	int receivedCommand = 0; // полученная от клиента команда
	iResult = recv(ClientSocket, (char*)&receivedCommand, sizeof(int), 0);
	if (iResult > 0) // Если получены данные
	{
		if (receivedCommand == COMMAND_GAME_STARTED) // Если код команды игра началась, то выводим сообщение об этом и идём далее
			printf("игра началась\n");
	}
	else if (iResult == 0)
	{
		printf("Connection closed");//соединение закрыто
		return 0;
	}
	else
	{
		printf("recv failed with error: %d\n", WSAGetLastError()); //функция завершилась с ошибкой
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	int randomNumber; // случайное число, которое мы ассоциируем с выбранной картой, чтобы от него посчитать хэш и передать клиенту (залог)

	while (cardsGone.size() != N - 1) // Пока не выбраны N - 1 карт
	{
		int pickedCard = -1; // выбранная карта
		do // в цикле просим ввести номер карты, пока не будет введён корректный
		{
			printf("необходимо выбрать карту из: ");
			for (int i = 0; i < N; i++) // выводятся имеющиеся карты. которые ещё не выбыли
				if (std::find(cardsGone.begin(), cardsGone.end(), i) == cardsGone.end()) // они не должны быть в массиве выбывших карт
				{
					printf("%d", i);
					if (i != N - 1) // Если карта не последня, то ставим запятую и пробел
						printf(", ");
				}
			printf(".\n"); // карты выведены
			scanf_s("%d", &pickedCard); // считываем номер загаданной карты
		} while (pickedCard < 0 || pickedCard >= N || std::find(cardsGone.begin(), cardsGone.end(), pickedCard) != cardsGone.end()); // пока не будет введён корректный номер (от 0 до N-1, карта всё ещё в игре)

		cardsGone.push_back(pickedCard); // карта была использована. запомним
		currentCommandCode = COMMAND_CARD_PICKED; // отправим команду о выборе карты
		iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
		/*находим случайное число, кратное номеру карты. 
		рассчитываем от случайного числа хэш и отправляем клиенту. 
		клиент угадывает карту, сервер отправляет случайное число.
		если оно кратно карте и хэш посчитанный клиентам совпадает с хэшом сервера, сервер не лжет*/
		randomNumber = associateRandomNumber(pickedCard); // получим случайное число, ассоциированное с номером карты
		int hash = computeHash(randomNumber); // рассчитаем хэш от него
		currentCommandCode = COMMAND_HASH; // и отправим клиенту заголовок сообщения Хэш идёт
		iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
		// А теперь и сам хэш
		iSendResult = send(ClientSocket, (char*)&hash, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

		// ждём выбор клиента
		printf("ожидание догадки килента...\n");

		receivedCommand = 0;
		// клиент что-то выбрал
		iResult = recv(ClientSocket, (char*)&receivedCommand, sizeof(int), 0);
		if (iResult > 0)
		{
			if (receivedCommand == COMMAND_CARD_PICKED) // если пришла команда Карта выбрана
			{
				int guessCard;
				iResult = recv(ClientSocket, (char*)&guessCard, sizeof(int), 0); // считываем номер карты
				if (iResult > 0)
				{
						printf("клиент считает, что это %d\n", guessCard);
						if (guessCard == pickedCard) // если выбор клиента и сервера совпал
						{
							currentCommandCode = COMMAND_CLIENT_RIGHT; // следующая команда клиенту будет Ты прав
							clientScore += N - cardsGone.size() + 1; // прибавляем очки клиенту
						}
						else
							currentCommandCode = COMMAND_CLIENT_WRONG; // иначе не угадал


						// отправим результат проверки догадки клиента
					iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						WSACleanup();
						return 1;
					}

					// Если отправили, что клиент не прав, то отправим номер загаданной карты
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

					// всегда отправляем в конце раунда случайное число, от которого считали хэш
					// сначала заголовок, что идёт это число
					currentCommandCode = COMMAND_RANDOMED;
					iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						WSACleanup();
						return 1;
					}
					// потом само число
					iSendResult = send(ClientSocket, (char*)&randomNumber, sizeof(int), 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						WSACleanup();
						return 1;
					}

					// выводим счёт клиента
					printf("счёт клиента: %d\n", clientScore);
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
		
		// и так в цикле
	}

	// цикл завершился, игра окончена, сообщим клиенту
	currentCommandCode = COMMAND_GAME_FINISHED;
	iSendResult = send(ClientSocket, (char*)&currentCommandCode, sizeof(int), 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}
	printf("игра окончена\n");




	// shutdown the connection since we're done
	//выключение соединения
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	//очистка
	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}
