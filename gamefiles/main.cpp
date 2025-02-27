#include "../kinesis/kinesis.h"

int main(int, char **)
{
    Kinesis::initialize();
    Kinesis::run();
}