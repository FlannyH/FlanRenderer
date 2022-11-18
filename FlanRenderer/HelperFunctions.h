#pragma once

void throw_fatal(std::string_view message) {
    std::cout << "[FATAL] ";
    std::cout << message;
    std::cout << std::endl;
    exit(1);
}

void throw_if_failed(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}