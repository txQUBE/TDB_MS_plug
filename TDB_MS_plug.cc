/*
 * Модуль имитации СУБТД
 */
#include <stdio.h>
#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/netmgr.h>

using namespace std;

bool shutDown = false;
bool stopChronometer = false;

#define REG_CHAN "RTS_registration_channel"
const int REG_TYPE = 101; //				тип сообщения для регистрации
const char *nodename = "localnode";

int tik_count = 0;//	Счетчик тиков

//Структура для запроса на регистрацию
typedef struct _pulse msg_header_t; // абстрактный тип для заголовка сообщения как у импульса
typedef struct _reg_data {
	msg_header_t hdr;
	string name; // 	имя СУБТД
	int pid; // 		id процесса
	pthread_t tid; // 	id нити
	int nd;
} reg_msg_t;

//Прототипы функций
static void* ChronometrService(void* arg);// нить хронометра СУБТД
void startChronometer(); // Функция запуска нити хронометра СУБТД
static void tick_handler(int sig);// Обработчик сигнала об истечении тика таймера
static void dead_handler(int sig);// Обработчик сигнала завершения процесса

string tdb_name;

/*
 * Основная нить main
 * 		Формирует имя СУБТД
 * 		Запускает нить регистрации в СРВ,
 * 			эта же нить получает сигналы СРВ об истечении тика таймера
 * 		Имеет меню для управления
 *
 * Аргументы:
 * [0] - path
 * [1] - порядковывй номер tdb для формирования имени
 */
int main(int argc, char* argv[]) {
	if (argc < 2) {
		cerr << "Usage: " << argv[0] << " <number>" << endl;
		return EXIT_FAILURE;
	}

	tdb_name = "TDB_MS_plug_" + string(argv[1]);

	cout << tdb_name << " starting..." << endl;
	pthread_t thread_id;
	if ((pthread_create(&thread_id, NULL, &ChronometrService, NULL)) != EOK) {
		cerr << tdb_name << ": error reg thread launch" << endl;
		exit(EXIT_FAILURE);
	};

	while (!shutDown) {
		int input;
		cin >> input;
		switch (input) {
		// Запустить нить хронометра
		case 1:{
			stopChronometer = false;
			startChronometer();
			break;
		}
		// Остановить нить хронометра
		case 2:
			stopChronometer = true;
			break;
		// Завершить процесс
		case 9:
			shutDown = true;
			break;
		}
	}
}

/**
 * Нить локальных часов СУБТД
 * Регистрируется в сервере СРВ
 * Принимает сигналы о прошедших тиках часов СРВ
 */
static void* ChronometrService(void*) {
	printf("Registration: starting...\n");

	// Установка обработчика сигнала
	signal(SIGUSR1, tick_handler);
	// Установка обработчика для SIGUSR2
	signal(SIGUSR2, dead_handler);

	reg_msg_t msg;

	/* Заголовок сообщения */
	msg.hdr.type = 0x00;
	msg.hdr.subtype = 0x00;
	msg.hdr.code = REG_TYPE;

	msg.name = tdb_name;
	msg.pid = getpid();
	msg.tid = pthread_self();
	msg.nd = 0;

	//	int nd = netmgr_strtond(nodename, NULL);
	//	if (nd == -1) {
	//		perror("ошибка получения дескриптора узла");
	//		exit(EXIT_FAILURE);
	//	}
	//	msg.nd = nd;


	cout << endl;
	cout << "Registration: ----msg--- " << endl;
	cout << "Registration: 	Name: " << msg.name << endl;
	cout << "Registration: 	PID : " << msg.pid << endl;
	cout << "Registration: 	TID : " << msg.tid << endl;
	cout << "Registration: 	ND	: " << msg.nd << endl;
	cout << "Registration: ----msg--- " << endl;
	cout << endl;

	// попытки подключения
	int server_coid = -1;
	while (server_coid == -1) {
		if ((server_coid = name_open(REG_CHAN, 0)) == -1) {
			cerr << "Registration: error name_open(REG_CHAN) errno: " << errno
					<< endl;
		}
		sleep(1);
	}

	cout << "Registration: sending registration data" << endl;
	int reply = MsgSend(server_coid, &msg, sizeof(msg), NULL, 0);
	switch(reply){
	case EOK:
		cout << tdb_name << " Success registration" << endl;
		break;
	case EINVAL:
		cout << tdb_name << " Registration error" << endl;
		break;
	default:
		cerr << "code " << errno;
		break;
	}

	/* Закрыть соединение с сервером */
	name_close(server_coid);

	// нить выполняется до принудительного завершения
	while (!stopChronometer) {
		// Имитация работы, нужна чтобы нить проверяла значение stopChronometer
		sleep(1);
	}

	cout << "Registration: EXIT" << endl;

	return EXIT_SUCCESS;
}

void tick_handler(int sig) {
	if (sig == SIGUSR1) {
		tik_count++;
		cout << tdb_name << " handler	: recieve SIGUSR1 !!!" << endl;
	}
}

// Обработчик сигнала терминирования
void dead_handler(int sig) {
	cout << tdb_name << "receive (" << sig << "). Terminating..." << endl;
	exit(0);
}

void startChronometer(){
	pthread_t thread_id;
	if ((pthread_create(&thread_id, NULL, &ChronometrService, NULL)) != EOK) {
		cerr << tdb_name << ": error reg thread launch" << endl;
		exit(EXIT_FAILURE);
	};
}
