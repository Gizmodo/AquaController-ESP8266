
#include "Scheduler.h"
Scheduler::~Scheduler() {
}

Scheduler::Scheduler() {
}

Device* Scheduler::getDevice() {
    return this->_device;
}

void Scheduler::setDevice(Device* device) {
    this->_device = device;
}

AlarmID_t Scheduler::getOn() {
    return _on;
}

void Scheduler::setOn(AlarmID_t on) {
    _on = on;
}

AlarmID_t Scheduler::getOff() {
    return _off;
}

void Scheduler::setOff(AlarmID_t off) {
    _off = off;
}