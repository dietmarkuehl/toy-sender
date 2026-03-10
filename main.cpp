// main.cpp                                                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <toy/execution.hpp>

namespace ex = toy;

// ----------------------------------------------------------------------------
namespace {
    struct receiver{
        auto set_value(auto value) { std::cout << "set_value: " << value << "\n"; return 0; }
        auto set_error(auto error) { std::cout << "set_error: " << error << "\n"; return 0; }
        auto set_stopped() { std::cout << "set_stopped\n"; return 0; }
    };
}

int main(int ac, char* av[])
{
    std::cout << std::unitbuf;
    std::cout << "Hello, world!\n";
    auto r0{ex::sync_wait(ex::just(42))};
    std::cout << "result: " << r0 << "\n";
    auto r1{ex::sync_wait([]()->ex::task<int> { co_return co_await ex::just(17); }())};
    std::cout << "result: " << r1 << "\n";
    ex::sync_wait([]()->ex::task<int> {
        for (std::size_t i{0}; true; ++i)
            co_await ex::just(i);
        co_return 0;
    }()); 
    return 0;
}
