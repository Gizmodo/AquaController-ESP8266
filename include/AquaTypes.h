#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include "TimeAlarms.h"
enum ledPosition { ONE, TWO, THREE, FOUR, FIVE, SIX };
enum doserType { K, NP, Fe };
typedef struct ledStruct_t {
    byte HOn, HOff;
    byte MOn, MOff;
    byte enabled;
    bool currentState;
    AlarmId on, off;
    byte pin;
} ledStruct;

typedef struct ledDescription_t {
    ledPosition position;
    String name;
    ledStruct_t led;
} ledDescription;

typedef struct ledState_t {
    String name;
    bool state;
} ledState;

typedef struct doser_t {
    byte dirPin;
    byte stepPin;
    byte enablePin;
    byte sleepPin;
    doserType type;
    String name;
    byte hour, minute;
    AlarmId alarm;
    byte volume;
    byte mode0_pin;
    byte mode1_pin;
    byte mode2_pin;
    byte index;
    word steps;
} doser;
typedef struct air_t {
     byte HOn, HOff;
    byte MOn, MOff;
    byte enablePin;
    byte sleepPin;
    doserType type;
    String name;
    byte hour, minute;
    AlarmId alarm;
    byte volume;
    byte mode0_pin;
    byte mode1_pin;
    byte mode2_pin;
    byte index;
    word steps;
} air;
inline const char* ToString(doserType v) {
    switch (v) {
        case K:
            return "K";
        case NP:
            return "NP";
        case Fe:
            return "Fe";
        default:
            return "Unknown";
    }
}
#if __cplusplus < 201402L
namespace std {
template <class T>
struct _Unique_if {
    typedef unique_ptr<T> _Single_object;
};

template <class T>
struct _Unique_if<T[]> {
    typedef unique_ptr<T[]> _Unknown_bound;
};

template <class T, size_t N>
struct _Unique_if<T[N]> {
    typedef void _Known_bound;
};

template <class T, class... Args>
typename _Unique_if<T>::_Single_object make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <class T>
typename _Unique_if<T>::_Unknown_bound make_unique(size_t n) {
    typedef typename remove_extent<T>::type U;
    return unique_ptr<T>(new U[n]());
}

template <class T, class... Args>
typename _Unique_if<T>::_Known_bound make_unique(Args&&...) = delete;
}  // namespace std
#endif

template <typename C, C beginVal, C endVal>
class Iterator {
    typedef typename std::underlying_type<C>::type val_t;
    int val;

   public:
    Iterator(const C& f) : val(static_cast<val_t>(f)) {
    }
    Iterator() : val(static_cast<val_t>(beginVal)) {
    }
    Iterator operator++() {
        ++val;
        return *this;
    }
    C operator*() {
        return static_cast<C>(val);
    }
    Iterator begin() {
        return *this;
    }  // default ctor is good
    Iterator end() {
        static const Iterator endIter = ++Iterator(endVal);  // cache it
        return endIter;
    }
    bool operator!=(const Iterator& i) {
        return val != i.val;
    }
};