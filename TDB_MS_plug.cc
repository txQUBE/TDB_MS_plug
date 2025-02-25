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
const int REG_TYPE = 101; //				��� ��������� ��� �����������
bool DEBUG = true;
bool ShutDown = false;
volatile sig_atomic_t stop_thread = 0;
const char *nodename = "localnode";

int tik_count = 0;//	������� �����

int mainPid = getpid();

//************��������� ��� ������� �� �����������********
typedef struct _pulse msg_header_t; // ����������� ��� ��� ��������� ��������� ��� � ��������
typedef struct _reg_data {
	msg_header_t hdr;
	string name; // ��� �����
	int pid; // 	id ��������
	int tid; // 	id �������
	int nd;
} reg_msg_t;

static void* realTimeServiceRegistration(void*);//						��������. ��������� ����� ���������� ������ �� �����������
static void handler(int sig);//						���������� ������� SignalKill


int main() {
	cout << "MainPid: " << mainPid << endl;

	if ((pthread_create(NULL, NULL, &realTimeServiceRegistration, NULL)) != EOK) {
		perror("Main: 	������ �������� ����\n");
		exit(EXIT_FAILURE);
	};

	while (!ShutDown) {

	}
}

//-------------------------------------------------------
//*****************��������******************************
//-------------------------------------------------------
static void* realTimeServiceRegistration(void*) {
	if (DEBUG) {
		printf("Client debug: Client is running...\n");
	}

	// ��������� ����������� �������
	signal(SIGUSR1, handler);

	reg_msg_t msg;

	/* ��������� ��������� ������� */
	msg.hdr.type = 0x00;
	msg.hdr.subtype = 0x00;
	msg.hdr.code = REG_TYPE;

	msg.name = "TemporalDB_MS_1";
	msg.pid = getpid();
	msg.tid = gettid();
//	int nd = netmgr_strtond(nodename, NULL);
//	if (nd == -1) {
//		perror("������ ��������� ����������� ����");
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
			cout << "Client 	: ������ ����������� � ������� �����������\n";
		}
		sleep(3);
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
