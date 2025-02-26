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

bool ShutDown = false;
volatile sig_atomic_t stop_thread = 0;

#define REG_CHAN "RTS_registration_channel"
const int REG_TYPE = 101; //				тип сообщения для регистрации
const char *nodename = "localnode";

int tik_count = 0;//	Счетчик тиков

int mainPid = getpid();

//************Структура для запроса на регистрацию********
typedef struct _pulse msg_header_t; // абстрактный тип для заголовка сообщения как у импульса
typedef struct _reg_data {
	msg_header_t hdr;
	string name; // 	имя СУБТД
	int pid; // 		id процесса
	pthread_t tid; // 	id нити
	int nd;
} reg_msg_t;

static void* realTimeServiceRegistration(void* arg);//	функция регистрации
static void handler(int sig);//	Обработчик сигнала SignalKill


/*
 * argc:
 * [1] - path
 * [2] - порядковывй номер tdb
 */
int main(int argc, char* argv[]) {
	if (argc < 2) {
		cerr << "Usage: " << argv[0] << " <number>" << endl;
		return EXIT_FAILURE;
	}

	cout << "TDB_MS_plug IS RUNNING" << endl;
	cout << "arg: "<< argv[0] << endl;
	cout << "arg: "<< argv[1] << endl;

	cout << "TDB: запуск нити регистрации" << endl;
	pthread_t thread_id;
	if ((pthread_create(&thread_id, NULL, &realTimeServiceRegistration, argv[1]))
			!= EOK) {
		perror("TDB_MS_plug: 	ошибка создания нити\n");
		exit(EXIT_FAILURE);
	};

	while (!ShutDown) {

	}
}

/**
 * Функция регистрации СУБТД в СРВ
 */
static void* realTimeServiceRegistration(void* arg) {
	printf("Registration: запущена\n");

	// Установка обработчика сигнала
	signal(SIGUSR1, handler);
	reg_msg_t msg;

	const char* tdb_id = (const char*) arg; // преобразование аргумента

	/* Заголовок сообщения */
	msg.hdr.type = 0x00;
	msg.hdr.subtype = 0x00;
	msg.hdr.code = REG_TYPE;

	msg.name = "TDB_MS_";
	msg.name += tdb_id;
	msg.pid = getpid();
	msg.tid = pthread_self();
	;
	//	int nd = netmgr_strtond(nodename, NULL);
	//	if (nd == -1) {
	//		perror("ошибка получения дескриптора узла");
	//		exit(EXIT_FAILURE);
	//	}
	//	msg.nd = nd;
	msg.nd = 0;
	cout << endl;
	cout << "Registration: ----msg--- " << endl;
	cout << "Registration: 	Name	: " << msg.name<< endl;
	cout << "Registration: 	PID 	: " << msg.pid << endl;
	cout << "Registration: 	TID 	: " << msg.tid << endl;
	cout << "Registration: 	ND	 	: " << msg.nd  << endl;
	cout << "Registration: ----msg--- " << endl;
	cout << endl;

	int server_coid = -1;
	// попытки подключения
	while (server_coid == -1) {
		if ((server_coid = name_open(REG_CHAN, 0)) == -1) {
			cout << "Registration: ошибка подключения к серверу регистрации\n";
		}
		sleep(1);
	}

	cout << "Client		: отправка данных для регистрации БТД" << endl;
	MsgSend(server_coid, &msg, sizeof(msg), NULL, 0);
	cout << "Данные отправлены\n";

	/* Закрыть соединение с сервером */
	//	name_close(server_coid);

	while (!stop_thread) {
	}

	return EXIT_SUCCESS;
}

void handler(int sig) {
	if (sig == SIGUSR1) {
		tik_count++;
		cout << "handler	: получен SIGUSR1 !!!" << endl;
		//		stop_thread = 1;
	}
}
