#include <iostream>

int main() {
    float f;
    const float* fp = (const float*)&f;
    int i = (int)f;
    return 0;
}
