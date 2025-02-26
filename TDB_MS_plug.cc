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
const int REG_TYPE = 101; //				��� ��������� ��� �����������
const char *nodename = "localnode";

int tik_count = 0;//	������� �����

int mainPid = getpid();

//************��������� ��� ������� �� �����������********
typedef struct _pulse msg_header_t; // ����������� ��� ��� ��������� ��������� ��� � ��������
typedef struct _reg_data {
	msg_header_t hdr;
	string name; // 	��� �����
	int pid; // 		id ��������
	pthread_t tid; // 	id ����
	int nd;
} reg_msg_t;

static void* realTimeServiceRegistration(void* arg);//	������� �����������
static void handler(int sig);//	���������� ������� SignalKill


/*
 * argc:
 * [1] - path
 * [2] - ����������� ����� tdb
 */
int main(int argc, char* argv[]) {
	if (argc < 2) {
		cerr << "Usage: " << argv[0] << " <number>" << endl;
		return EXIT_FAILURE;
	}

	cout << "TDB_MS_plug IS RUNNING" << endl;
	cout << "arg: "<< argv[0] << endl;
	cout << "arg: "<< argv[1] << endl;

	cout << "TDB: ������ ���� �����������" << endl;
	pthread_t thread_id;
	if ((pthread_create(&thread_id, NULL, &realTimeServiceRegistration, argv[1]))
			!= EOK) {
		perror("TDB_MS_plug: 	������ �������� ����\n");
		exit(EXIT_FAILURE);
	};

	while (!ShutDown) {

	}
}

/**
 * ������� ����������� ����� � ���
 */
static void* realTimeServiceRegistration(void* arg) {
	printf("Registration: ��������\n");

	// ��������� ����������� �������
	signal(SIGUSR1, handler);
	reg_msg_t msg;

	const char* tdb_id = (const char*) arg; // �������������� ���������

	/* ��������� ��������� */
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
	//		perror("������ ��������� ����������� ����");
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
	// ������� �����������
	while (server_coid == -1) {
		if ((server_coid = name_open(REG_CHAN, 0)) == -1) {
			cout << "Registration: ������ ����������� � ������� �����������\n";
		}
		sleep(1);
	}

	cout << "Client		: �������� ������ ��� ����������� ���" << endl;
	MsgSend(server_coid, &msg, sizeof(msg), NULL, 0);
	cout << "������ ����������\n";

	/* ������� ���������� � �������� */
	//	name_close(server_coid);

	while (!stop_thread) {
	}

	return EXIT_SUCCESS;
}

void handler(int sig) {
	if (sig == SIGUSR1) {
		tik_count++;
		cout << "handler	: ������� SIGUSR1 !!!" << endl;
		//		stop_thread = 1;
	}
}
