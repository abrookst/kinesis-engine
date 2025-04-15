#include "../kinesis/kinesis.h"

int main(int /*argc*/, char **/*argv*/)
{
    Kinesis::initialize();
    //Kinesis::LoadScene(scene)
    while(Kinesis::run()){
        //Conditional for scene, //Kinesis::LoadScene(scene2)
        //event handling
        //can create a class Player inherits Kinesis::GameObject, has a Kinesis::Camera, etc.
    }
}