#include "TimeAlarms.h"

enum ledPosition { LEFT, CENTER, RIGHT };
typedef struct ledStruct_t {
    byte HOn, HOff;
    byte MOn, MOff;
    byte enabled;
    byte currentState;
    AlarmId on, off;
    byte pin;
} ledStruct;

typedef struct ledDescription_t {
    ledPosition position;
    String name;
    ledStruct_t led;
} ledDescription;