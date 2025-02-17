#ifndef DISRUPTORS_H
#define DISRUPTORS_H

extern unsigned long lastInterrupt;
extern bool flashActive;
extern unsigned long flashStart;

void disruptorTask();

#endif // DISRUPTORS_H
