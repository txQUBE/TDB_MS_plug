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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>

using namespace std;

bool shutDown = false;
bool stopChronometer = false;

#define REG_CHAN "rts_registration_channel"
#define RTS_SHM_TIME_NAME "rts_shm_time"

#define TICKPAS_SIG SIGRTMIN // сигнал о прошедшем тике
#define TERM_SIG SIGRTMAX // сигнал терминировать заглушку
const int REG_TYPE = 101; // тип сообщения для регистрации
const char *nodename = "localnode";

// Структура для буфера локальных часов
typedef struct {
	int count; // 	Номер текущего тика
	long tick_nsec; // 	Длительность одного тика в наносекундах
	int tick_sec;// 	Длительность одного тика в секундах
} local_time;

// Структура именованной памяти параметров таймера
typedef struct {
	pthread_rwlockattr_t attr; //атрибутная запись блокировки чтения/записи
	pthread_rwlock_t rwlock; //блокировка чтения/записи
	local_time Time; // 	Номер текущего тика
} shm_time;

typedef struct _pulse msg_header_t; // абстрактный тип для заголовка сообщения как у импульса
//Структура для запроса на регистрацию
typedef struct {
	msg_header_t hdr;
	string name; // 	имя СУБТД
	int pid; // 		id процесса
	pthread_t tid; // 	id нити
	int nd;
} reg_msg;

string tdb_name; // имя СУБТД
local_time localTime; // локальные часы СУБТД

//Прототипы функций
shm_time* connectToNamedMemory(const char* name);
static void* rtsReg(void* arg);// нить хронометра СУБТД
pthread_t startRtsReg(); // Функция запуска нити хронометра СУБТД
static void tick_handler(int sig);// Обработчик сигнала об истечении тика таймера
static void dead_handler(int sig);// Обработчик сигнала завершения процесса
void showLocalTime();

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
	pthread_t rtsReg_thread = startRtsReg();

	while (!shutDown) {
		int input;
		cin >> input;
		switch (input) {
		case 1: { // Запустить нить хронометра
			stopChronometer = false;
			startRtsReg();
			break;
		}
		case 2: //показать локальные часы
			showLocalTime();
			break;
		case 8: // Остановить нить хронометра
			stopChronometer = true;
			break;

		case 9: // Завершить процесс
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
static void* rtsReg(void*) {
	printf("Registration: starting...\n");

	// Установка обработчика сигнала
	signal(TICKPAS_SIG, tick_handler);
	// Установка обработчика для SIGUSR2
	signal(TERM_SIG, dead_handler);

	reg_msg msg;

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
	switch (reply) {
	case EOK:
		cout << tdb_name << " Success registration" << endl;
		break;
	case EINVAL:
		cout << tdb_name << " Registration error" << endl;
		break;
	default:
		cerr << tdb_name << " MsgSend error, errno: " << errno;
		break;
	}
	// закрыть соединение с сервером
	name_close(server_coid);

	// нить выполняется до принудительного завершения
	while (!stopChronometer) {
		// имитация работы, нужна чтобы нить проверяла значение stopChronometer
		sleep(1);
	}

	cout << "Registration: EXIT" << endl;

	return EXIT_SUCCESS;
}

/*
 * Обработчик сигналов таймера
 */
void tick_handler(int sig) {
	if (sig == TICKPAS_SIG) {
		localTime.count++;
		cout << tdb_name << " handler	: recieve SIGUSR1 !!!" << endl;
		cout << "localTime count: " << localTime.count << endl;
	}
}

/*
 * Обработчик сигнала терминирования
 */
void dead_handler(int sig) {
	if (sig == TERM_SIG) {
		cout << tdb_name << "receive (" << sig << "). Terminating..." << endl;
		exit(0);
	}

}

/*
 * функция запуска локальных часов СУБТД
 */
pthread_t startRtsReg() {
	pthread_t thread_id;
	if ((thread_id = pthread_create(&thread_id, NULL, &rtsReg, NULL)) != EOK) {
		cerr << tdb_name << ": error reg thread launch" << endl;
		exit(EXIT_FAILURE);
	};
	return thread_id;
}

void showLocalTime() {
	cout << tdb_name << " local time: " << endl;
	cout << "Tick count: " << localTime.count << endl;
	cout << "Tick value: " << localTime.tick_sec << " sec"
			<< localTime.tick_nsec << " nsec" << endl;
}

void getTimerUpdate() {
	shm_time* shmTimePtr = connectToNamedMemory(RTS_SHM_TIME_NAME);
	pthread_rwlock_rdlock(&shmTimePtr->rwlock);
	localTime.tick_nsec = shmTimePtr->Time.tick_nsec;
	localTime.tick_sec = shmTimePtr->Time.tick_sec;
	localTime.count = 0;
	pthread_rwlock_unlock(&shmTimePtr->rwlock);
}

/*
 * Производит подключение к именованной памяти.
 */
shm_time* connectToNamedMemory(const char* name) {
	shm_time* namedMemoryPtr;
	int fd;
	// Открыть именованную память
	if ((fd = shm_open(name, O_RDWR, 0777)) == -1) {
		cerr << tdb_name << " error shm_open, errno: " << errno << endl;
		exit(0);
	}
	//Отображение разделяемой именованной памяти в адресное пространство процесса
	if ((namedMemoryPtr = (shm_time*) mmap(NULL, sizeof(shm_time), PROT_READ
			| PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		cerr << tdb_name << " error mmap, errno: " << errno << endl;
		exit(0);
	}

	return namedMemoryPtr;
}
