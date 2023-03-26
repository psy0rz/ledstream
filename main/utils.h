//
// Created by psy on 3/26/23.
//

#ifndef LEDSTREAM_UTILS_H
#define LEDSTREAM_UTILS_H

bool duty_cycle(unsigned long on, unsigned long total, unsigned long starttime = 0) {
    if (!starttime)
        return ((millis() % total) < on);
    else
        return (((millis() - starttime) % total) < on);
}

#endif //LEDSTREAM_UTILS_H
