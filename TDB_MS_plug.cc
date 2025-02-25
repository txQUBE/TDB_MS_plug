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

#define REG_CHAN "RTS_registration_channel"
const int REG_TYPE = 101; //				тип сообщения для регистрации
bool DEBUG = true;
bool ShutDown = false;
volatile sig_atomic_t stop_thread = 0;
const char *nodename = "localnode";

int tik_count = 0;//	Счетчик тиков

int mainPid = getpid();

//************Структура для запроса на регистрацию********
typedef struct _pulse msg_header_t; // абстрактный тип для заголовка сообщения как у импульса
typedef struct _reg_data {
	msg_header_t hdr;
	string name; // имя СУБТД
	int pid; // 	id процесса
	int tid; // 	id таймера
	int nd;
} reg_msg_t;

static void* realTimeServiceRegistration(void*);//						Заглушка. Имитирует СУБТД посылающую запрос на регистрацию
static void handler(int sig);//						Обработчик сигнала SignalKill


int main() {
	cout << "MainPid: " << mainPid << endl;

	if ((pthread_create(NULL, NULL, &realTimeServiceRegistration, NULL)) != EOK) {
		perror("Main: 	ошибка создания нити\n");
		exit(EXIT_FAILURE);
	};

	while (!ShutDown) {

	}
}

//-------------------------------------------------------
//*****************Заглушка******************************
//-------------------------------------------------------
static void* realTimeServiceRegistration(void*) {
	if (DEBUG) {
		printf("Client debug: Client is running...\n");
	}

	// Установка обработчика сигнала
	signal(SIGUSR1, handler);

	reg_msg_t msg;

	/* Заголовок сообщения клиента */
	msg.hdr.type = 0x00;
	msg.hdr.subtype = 0x00;
	msg.hdr.code = REG_TYPE;

	msg.name = "TemporalDB_MS_1";
	msg.pid = getpid();
	msg.tid = gettid();
//	int nd = netmgr_strtond(nodename, NULL);
//	if (nd == -1) {
//		perror("ошибка получения дескриптора узла");
//		exit(EXIT_FAILURE);
//	}
//	msg.nd = nd;
	msg.nd = 0;

	cout << endl << "Registration	  	: ----msg---" << endl;
	cout << "Registration		: 	Name		: " << msg.name << endl;
	cout << "Registration		: 	Process ID 	: " << msg.pid << endl;
	cout << "Registration	  	: 	Thread ID 	:  " << msg.tid << endl;
	cout << "Registration	  	: 	 ND	 		:  " << msg.nd << endl << endl;
	cout << "Registration	  	: ----msg---" << endl << endl;

	int server_coid = -1;

	while (server_coid == -1) {
		if ((server_coid = name_open(REG_CHAN, 0)) == -1) {
			cout << "Client 	: ошибка подключения к серверу регистрации\n";
		}
		sleep(3);
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
