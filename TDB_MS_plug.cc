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

#include <ChronTimer.h>

using namespace std;

class TDBMS;
static TDBMS* tdbPtr;
static ChronTimer* localTimerPtr;

static const int SIG_TICK = SIGRTMIN + 1;
static const int SIG_TICK_MANUAL = SIGRTMIN + 2;
static const int SIG_TIME_DATA_UPDATED = SIGRTMIN + 3;
static const int SIG_CHRONOMETR_DISCONNECTED = SIGRTMIN + 9;
static const int SIG_TERM = SIGUSR2;

static const string NODENAME1 = "/net/qnx1";
static const string NODENAME2 = "/net/qnx2";
static const string REG_CHAN = "RTS_registration_channel";

static const int REG_TYPE = 101;

struct RegistrationMessage {
	_pulse hdr;
	char name[255];
	int pid;
	pthread_t tid;
	char nd[255];

	long tick_nsec;
	int tick_sec;
	int time;
};

class TDBMS {
private:
	static bool shouldShutdown_;

	string name_;
	int ndParity_;
	bool isRunning_;

	static void* chronometerThread(void* context) {
		return ((TDBMS*) context)->chronometerService();
	}

	// Обработчик сигналов
	static void signalHandler(int sig, siginfo_t* info, void* context) {

		cout << "SIGNAL RECEIVE sig: " << sig << endl;

		switch (sig) {
		case SIG_TICK:
			localTimerPtr->timeIncrease();
			cout << tdbPtr->name_ << " Receive tick №"
					<< localTimerPtr->getTime() << endl;
			break;

		case SIG_TICK_MANUAL:
			localTimerPtr->timeManualIncrease();
			cout << tdbPtr->name_ << " Receive Manual tick №"
					<< localTimerPtr->getTimeManual() << endl;
			break;

		case SIG_TIME_DATA_UPDATED:
			tdbPtr->sendRegistration();
			break;

		case SIG_TERM:
			tdbPtr->shutdown();
			break;

		case SIG_CHRONOMETR_DISCONNECTED:
			tdbPtr->stopChronometer();
			break;
		default:
			cout << " Signal handler receive unknown signal: " << sig << endl;
			break;
		}
	}

	void setupSignalHandlers() {
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = signalHandler;

		// Устанавливаем обработчики для всех сигналов
		sigaction(SIG_TICK, &sa, NULL);
		sigaction(SIG_TICK_MANUAL, &sa, NULL);
		sigaction(SIG_TIME_DATA_UPDATED, &sa, NULL);
		sigaction(SIG_CHRONOMETR_DISCONNECTED, &sa, NULL);
		sigaction(SIG_TERM, &sa, NULL);
	}

	template<size_t N>
	void stringToCharArray(const std::string& str, char(&dest)[N]) {
		strncpy(dest, str.c_str(), N - 1);
		dest[N - 1] = '\0';
	}

	void* chronometerService() {
		cout << "Registration: starting..." << endl;

		setupSignalHandlers();
		localTimerPtr = new ChronTimer(0, 0, 0);

		if (ndParity_ == -1) {
			cerr << name_ << " Parity determining error" << endl;
			exit(EXIT_FAILURE);
		}

		if (ndParity_ == 0) {
			int server_nd = netmgr_strtond(NODENAME1.c_str(), NULL);
			if (server_nd == -1) {
				cerr << "netmgr_strtond for server ND error, errno: " << errno
						<< endl;
				exit(EXIT_FAILURE);
			}

			int coid = ConnectAttach(server_nd, 0, 0, 0, 0);
			if (coid == -1) {
				cerr << "error ConnectAttach (errno: " << errno << ")" << endl;
				exit(EXIT_FAILURE);
			}
		}

		if (!shouldShutdown_) {
			int server_coid = sendRegistration();
			name_close(server_coid);
		}

		while (!shouldShutdown_ && isRunning_) {
			sleep(1);
		}

		cout << "Chronometr: chronometr disconnected" << endl;

		//clean ChronTimer resources
		delete localTimerPtr;
		localTimerPtr = NULL;

		return NULL;
	}

	void printRegistrationInfo(const RegistrationMessage& msg) {
		cout << endl << "Registration: ----msg--- " << endl;
		cout << "Registration:  Name: " << msg.name << endl;
		cout << "Registration:  PID : " << msg.pid << endl;
		cout << "Registration:  TID : " << msg.tid << endl;
		cout << "Registration:  ND  : " << msg.nd << endl;
		cout << "Registration: ----msg--- " << endl << endl;
	}

	int sendRegistration() {
		RegistrationMessage msg;
		msg.hdr.type = 0x00;
		msg.hdr.subtype = 0x00;
		msg.hdr.code = REG_TYPE;
		stringToCharArray(name_, msg.name);
		msg.pid = getpid();
		msg.tid = pthread_self();
		stringToCharArray((ndParity_ == 1) ? NODENAME1 : NODENAME2, msg.nd);

		printRegistrationInfo(msg);

		int server_coid = -1;
		while (server_coid == -1 && !shouldShutdown_) {
			server_coid = name_open(REG_CHAN.c_str(), 0);
			if (server_coid == -1) {
				cerr << "Registration: error name_open(REG_CHAN) errno: "
						<< errno << endl;
				sleep(1);
			}
		}

		cout << "Registration: sending registration data" << endl;
		RegistrationMessage reply;
		int status = MsgSend(server_coid, &msg, sizeof(msg), &reply,
				sizeof(reply));

		switch (status) {
		case EOK:
			cout << name_ << " Success registration" << endl;

			localTimerPtr->updateTimer(reply.tick_nsec, reply.tick_sec,
					reply.time);

			cout << name_ << " ";
			localTimerPtr->print();
			break;
		case EINVAL:
			cout << name_ << " Registration error" << endl;
			break;
		default:
			cerr << name_ << " receive unknown reply status " << status
					<< ". Error: " << strerror(errno);
			break;
		}

		return server_coid;
	}

public:
	TDBMS(const string& name, int number) :
		name_(name), ndParity_(number % 2), isRunning_(false) {
	}

	void start() {
		isRunning_ = true;

		pthread_t thread_id;
		if (pthread_create(&thread_id, NULL, &chronometerThread, this) != EOK) {
			cerr << name_ << ": error ChronometerThread launch: " << strerror(
					errno) << endl;
			exit(EXIT_FAILURE);
		}
	}

	void stopChronometer() {
		isRunning_ = false;
	}

	void shutdown() {
		shouldShutdown_ = true;
	}

	bool shouldShutdown() {
		return shouldShutdown_;
	}

	string getName() {
		return name_;
	}
};

bool TDBMS::shouldShutdown_ = false;

void showMenuList() {
	cout << "0. Show menu list\n";
	cout << "1. Start chronometer service\n";
	cout << "2. Stop chronometer service\n";
	cout << "3. Print LocalTimer info\n";
	cout << "999. Shut down application\n";
}

void handleInput(int input) {
	switch (input) {
	case 0:
		showMenuList();
		break;
	case 1:
		tdbPtr->start();
		break;
	case 2:
		tdbPtr->stopChronometer();
		break;
	case 3:
		if (localTimerPtr != NULL) {
			cout << tdbPtr->getName() << " ";
			localTimerPtr->print();
		} else
			cout << tdbPtr->getName() << " Timer doesen't exist." << endl;
		break;
	case 999:
		tdbPtr->shutdown();
		break;
	}
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cerr << "Usage: " << argv[0] << " <number>" << endl;
		return EXIT_FAILURE;
	}

	string tdbName = "TDB_MS_plug_" + string(argv[1]);
	TDBMS tdb(tdbName, atoi(argv[1]));

	tdbPtr = &tdb;

	cout << tdbName << " starting..." << endl;
	tdb.start();

	cout << "Enter 0 to show menu list\n";

	while (!tdb.shouldShutdown()) {
		int input;
		cin >> input;
		handleInput(input);
	}

	cout << tdb.getName() << " is shutting down" << endl;

	return EXIT_SUCCESS;
}
