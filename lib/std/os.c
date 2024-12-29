#include "mocha.h"

#include <unistd.h>

null __os__sleep__(i32 seconds)
{
    sleep(seconds);
}
