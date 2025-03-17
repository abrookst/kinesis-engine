#include "../kinesis/kinesis.h"

int main(int /*argc*/, char **/*argv*/)
{
    Kinesis::initialize();
    Kinesis::run();
}