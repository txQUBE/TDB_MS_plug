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
#define TICKPAS_SIG SIGRTMIN // ������ � ��������� ����
#define TERM_SIG SIGRTMAX // ������ ������������� ��������
#define TICKUPDATE_SIG SIGRTMIN + 1 // ������ �� ���������� ���� �������
#define TICKUPDATE_CODE 2 // ��� ��������� � ����������� �������
const int REG_TYPE = 101; // ��� ��������� ��� �����������
const char *nodename = "localnode";

typedef struct _pulse msg_header_t; // ����������� ��� ��� ��������� ��������� ��� � ��������

// ��������� ��� ������ ��������� �����
struct local_time_t {
	pthread_mutex_t mtx;
	long tick_nsec; // 	������������ ������ ���� � ������������
	int tick_sec;// 	������������ ������ ���� � ��������
	int Time; // 		����� �������� ���� ����� ���
};

struct local_time_msg_t {
	msg_header_t hdr;
	long tick_nsec; // 	������������ ������ ���� � ������������
	int tick_sec;// 	������������ ������ ���� � ��������
	int Time; // 		����� �������� ���� ����� ���
};

local_time_t local_time;

//��������� ��� ������� �� �����������

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
int getTimerUpdate(); // ������� ��������� ���������� �������

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

	if (pthread_mutex_init(&local_time.mtx, NULL) != EOK) {
		cerr << "TDBMS " << argv[1] << " "
				<< "pthread_mutex_init() error, errno: " << errno << endl;
	}

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
		case 1: {
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
	signal(TICKPAS_SIG, tick_handler);
	// ��������� ����������� ��� SIGUSR2
	signal(TERM_SIG, dead_handler);

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

	// �������� ��������� �������
	getTimerUpdate();

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
		pthread_mutex_lock(&local_time.mtx);
		local_time.Time++;
		cout << tdb_name << " handler	: recieve SIGUSR1 !!!" << endl;
		pthread_mutex_unlock(&local_time.mtx);
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
 * ���������� �������� ���������� �������
 */
void tick_update_handler(int sig) {
	if (sig == TICKUPDATE_SIG) {
		getTimerUpdate();
	}
}

/*
 * ������� ������� ��������� ����� �����
 */
void startChronometer() {
	pthread_t thread_id;
	if ((pthread_create(&thread_id, NULL, &ChronometrService, NULL)) != EOK) {
		cerr << tdb_name << ": error reg thread launch" << endl;
		exit(EXIT_FAILURE);
	};
}

/*
 * ������� ���������� ���������� �������
 */
int getTimerUpdate() {
	local_time_msg_t msg;
	name_attach_t* attach;
	if ((attach = name_attach(NULL, tdb_name.c_str(), 0)) == NULL) {
		cerr << tdb_name << ": 	error name_attach(). errno:" << errno << endl;
		return EXIT_FAILURE;
	}

	while (true) {
		int rcvid;
		rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
		if (rcvid == -1) {
			if (errno == ENOTCONN) {
				// ������ ����������
				cerr << tdb_name << "MsgReveive error, errno: " << errno
						<< endl;
				ConnectDetach(msg.hdr.scoid);
				name_detach(attach, 0);
				return EXIT_SUCCESS;
			} else {
				cerr << "error MsgReceive, errno: " << errno << endl;
				return EXIT_FAILURE;
			}

			/* ���������� ��������� �� ������� */
			// ��������� ��������� ���������
			if (msg.hdr.type == _IO_CONNECT) {
				MsgReply(rcvid, EOK, NULL, 0);
				continue;
			}
			if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX) { //�������� IO-��������� �� ����
				MsgError(rcvid, ENOSYS );
				continue;
			}

			if (msg.hdr.code == TICKUPDATE_CODE) {
				pthread_mutex_lock(&local_time.mtx);
				local_time.Time = msg.Time;
				local_time.tick_nsec = msg.tick_nsec;
				local_time.tick_sec = msg.tick_sec;
				MsgReply(rcvid, EOK, 0, 0);
				pthread_mutex_unlock(&local_time.mtx);
				continue;
			}
		}
	}
	return NULL;
}
