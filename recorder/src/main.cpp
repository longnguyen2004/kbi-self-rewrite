#include <recorder.h>
#include "controller/controller.h"

int main(int argc, char const *argv[])
{
    Recorder rec;
    ConsoleController controller(rec);
    controller.Run();
    return 0;
}
