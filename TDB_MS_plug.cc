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

using namespace std;

bool shutDown = false;
bool stopChronometer = false;

#define REG_CHAN "RTS_registration_channel"
const int REG_TYPE = 101; //				��� ��������� ��� �����������
const char *nodename = "localnode";

int tik_count = 0;//	������� �����

//��������� ��� ������� �� �����������
typedef struct _pulse msg_header_t; // ����������� ��� ��� ��������� ��������� ��� � ��������
typedef struct _reg_data {
	msg_header_t hdr;
	string name; // 	��� �����
	int pid; // 		id ��������
	pthread_t tid; // 	id ����
	int nd;
} reg_msg_t;

//��������� �������
static void* ChronometrService(void* arg);// ���� ���������� �����
void startChronometer(); // ������� ������� ���� ���������� �����
static void tick_handler(int sig);// ���������� ������� �� ��������� ���� �������
static void dead_handler(int sig);// ���������� ������� ���������� ��������

string tdb_name;

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
	pthread_t thread_id;
	if ((pthread_create(&thread_id, NULL, &ChronometrService, NULL)) != EOK) {
		cerr << tdb_name << ": error reg thread launch" << endl;
		exit(EXIT_FAILURE);
	};

	while (!shutDown) {
		int input;
		cin >> input;
		switch (input) {
		// ��������� ���� ����������
		case 1:{
			stopChronometer = false;
			startChronometer();
			break;
		}
		// ���������� ���� ����������
		case 2:
			stopChronometer = true;
			break;
		// ��������� �������
		case 9:
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
static void* ChronometrService(void*) {
	printf("Registration: starting...\n");

	// ��������� ����������� �������
	signal(SIGUSR1, tick_handler);
	// ��������� ����������� ��� SIGUSR2
	signal(SIGUSR2, dead_handler);

	reg_msg_t msg;

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

	/* ������� ���������� � �������� */
	name_close(server_coid);

	// ���� ����������� �� ��������������� ����������
	while (!stopChronometer) {
		// �������� ������, ����� ����� ���� ��������� �������� stopChronometer
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

// ���������� ������� ��������������
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
