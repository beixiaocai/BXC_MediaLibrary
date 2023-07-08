#include "test.h"
#include <thread>

int main()
{
    //std::thread([]() {
    //	test_AudioRecorder();
    //	}).detach();
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //test_AudioRecorder();
    test_VideoRecorder();

    system("pause");


    return 0;
}
