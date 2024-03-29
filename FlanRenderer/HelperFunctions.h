#pragma once
#include <string>
#include <cstdio>
#include <iostream>

static void throw_fatal(std::string message) {
    std::cout << "[FATAL] ";
    std::cout << message;
    std::cout << std::endl;
    exit(1);
}

static void throw_if_failed(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}

