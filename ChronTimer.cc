#include "ChronTimer.h"

ChronTimer::ChronTimer() : tick_nsec(0), tick_sec(0), time(0) {}

ChronTimer::ChronTimer(long ns, int sec, int t)
    : tick_nsec(ns), tick_sec(sec), time(t) {}

long ChronTimer::getTickNano() const { return tick_nsec; }
int ChronTimer::getTickSec() const { return tick_sec; }
int ChronTimer::getTime() const { return time; }
int ChronTimer::getTimeManual() const { return timeManual; }

void ChronTimer::setTickNano(long ns) { tick_nsec = ns; }
void ChronTimer::setTickSec(int sec) { tick_sec = sec; }
void ChronTimer::setTime(int t) { time = t; }
void ChronTimer::updateTimer(long tick_nsec, int tick_sec, int Time) {
	setTickNano(tick_nsec);
	setTickSec(tick_sec);
	setTime(Time);
}

void ChronTimer::timeIncrease() {
    time++;
}

void ChronTimer::timeManualIncrease() {
    timeManual++;
}

void ChronTimer::reset() {
    tick_nsec = 0;
    tick_sec = 0;
    time = 0;
    timeManual = 0;
}

void ChronTimer::print() const {
    std::cout << "Timer: " << tick_sec << " sec "
              << tick_nsec << " nsec | Time: " << time << std::endl
              << "Manual tick count: " << timeManual << std::endl;
}
