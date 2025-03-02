/*
 * ������ �������� �����
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

#define TICKPAS_SIG SIGRTMIN // ������ � ��������� ����
#define TERM_SIG SIGRTMAX // ������ ������������� ��������
const int REG_TYPE = 101; // ��� ��������� ��� �����������
const char *nodename = "localnode";

// ��������� ��� ������ ��������� �����
typedef struct {
	int count; // 	����� �������� ����
	long tick_nsec; // 	������������ ������ ���� � ������������
	int tick_sec;// 	������������ ������ ���� � ��������
} local_time;

// ��������� ����������� ������ ���������� �������
typedef struct {
	pthread_rwlockattr_t attr; //���������� ������ ���������� ������/������
	pthread_rwlock_t rwlock; //���������� ������/������
	local_time Time; // 	����� �������� ����
} shm_time;

typedef struct _pulse msg_header_t; // ����������� ��� ��� ��������� ��������� ��� � ��������
//��������� ��� ������� �� �����������
typedef struct {
	msg_header_t hdr;
	string name; // 	��� �����
	int pid; // 		id ��������
	pthread_t tid; // 	id ����
	int nd;
} reg_msg;

string tdb_name; // ��� �����
local_time localTime; // ��������� ���� �����

//��������� �������
shm_time* connectToNamedMemory(const char* name);
static void* rtsReg(void* arg);// ���� ���������� �����
pthread_t startRtsReg(); // ������� ������� ���� ���������� �����
static void tick_handler(int sig);// ���������� ������� �� ��������� ���� �������
static void dead_handler(int sig);// ���������� ������� ���������� ��������
void showLocalTime();

/*
 * �������� ���� main
 * 		��������� ��� �����
 * 		��������� ���� ����������� � ���,
 * 			��� �� ���� �������� ������� ��� �� ��������� ���� �������
 * 		����� ���� ��� ����������
 *
 * ���������:
 * [0] - path
 * [1] - ����������� ����� tdb ��� ������������ �����
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
		case 1: { // ��������� ���� ����������
			stopChronometer = false;
			startRtsReg();
			break;
		}
		case 2: //�������� ��������� ����
			showLocalTime();
			break;
		case 8: // ���������� ���� ����������
			stopChronometer = true;
			break;

		case 9: // ��������� �������
			shutDown = true;
			break;
		}
	}
}

/**
 * ���� ��������� ����� �����
 * �������������� � ������� ���
 * ��������� ������� � ��������� ����� ����� ���
 */
static void* rtsReg(void*) {
	printf("Registration: starting...\n");

	// ��������� ����������� �������
	signal(TICKPAS_SIG, tick_handler);
	// ��������� ����������� ��� SIGUSR2
	signal(TERM_SIG, dead_handler);

	reg_msg msg;

	/* ��������� ��������� */
	msg.hdr.type = 0x00;
	msg.hdr.subtype = 0x00;
	msg.hdr.code = REG_TYPE;

	msg.name = tdb_name;
	msg.pid = getpid();
	msg.tid = pthread_self();
	msg.nd = 0;

	//	int nd = netmgr_strtond(nodename, NULL);
	//	if (nd == -1) {
	//		perror("������ ��������� ����������� ����");
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

	// ������� �����������
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
	// ������� ���������� � ��������
	name_close(server_coid);

	// ���� ����������� �� ��������������� ����������
	while (!stopChronometer) {
		// �������� ������, ����� ����� ���� ��������� �������� stopChronometer
		sleep(1);
	}

	cout << "Registration: EXIT" << endl;

	return EXIT_SUCCESS;
}

/*
 * ���������� �������� �������
 */
void tick_handler(int sig) {
	if (sig == TICKPAS_SIG) {
		localTime.count++;
		cout << tdb_name << " handler	: recieve SIGUSR1 !!!" << endl;
		cout << "localTime count: " << localTime.count << endl;
	}
}

/*
 * ���������� ������� ��������������
 */
void dead_handler(int sig) {
	if (sig == TERM_SIG) {
		cout << tdb_name << "receive (" << sig << "). Terminating..." << endl;
		exit(0);
	}

}

/*
 * ������� ������� ��������� ����� �����
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
 * ���������� ����������� � ����������� ������.
 */
shm_time* connectToNamedMemory(const char* name) {
	shm_time* namedMemoryPtr;
	int fd;
	// ������� ����������� ������
	if ((fd = shm_open(name, O_RDWR, 0777)) == -1) {
		cerr << tdb_name << " error shm_open, errno: " << errno << endl;
		exit(0);
	}
	//����������� ����������� ����������� ������ � �������� ������������ ��������
	if ((namedMemoryPtr = (shm_time*) mmap(NULL, sizeof(shm_time), PROT_READ
			| PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		cerr << tdb_name << " error mmap, errno: " << errno << endl;
		exit(0);
	}

	return namedMemoryPtr;
}
