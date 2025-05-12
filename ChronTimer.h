#ifndef CHRONTIMER_HPP
#define CHRONTIMER_HPP

#include <iostream>

class ChronTimer {
private:
    long tick_nsec;
    int tick_sec;
    int time;
    int timeManual;

public:
    ChronTimer();
    ChronTimer(long ns, int sec, int t);

    long getTickNano() const;
    int getTickSec() const;
    int getTime() const;
    int getTimeManual() const;

    void setTickNano(long ns);
    void setTickSec(int sec);
    void setTime(int t);
    void updateTimer(long tick_nsec, int tick_sec, int Time);

    void timeIncrease();
    void timeManualIncrease();
    void reset();
    void print() const;
};

#endif // CHRONTIMER_HPP
