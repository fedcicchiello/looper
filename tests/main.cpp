#include "TestUtil.h"

int main()
{
    int ran = 0;
    for (auto& t : testing::registry())
    {
        const int before = testing::failures();
        t.fn();
        ++ran;
        std::cout << (testing::failures() == before ? "[ ok ] " : "[FAIL] ") << t.name << "\n";
    }
    std::cout << ran << " tests, " << testing::failures() << " failed checks\n";
    return testing::failures() == 0 ? 0 : 1;
}
