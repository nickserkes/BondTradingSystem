#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>

#ifndef HELPERFUNCTION_HPP
#define HELPERFUNCTION_HPP

std::string convert_to_fractional(double price){
    std::stringstream converted_ss;
    converted_ss << floor(price+0.0000001) << '-';
    converted_ss << std::setw(2) << std::setfill('0') << (int)(((int) std::round(price * 256.0) % 256)/8);
    int last_digit = (int)((int) std::round(price * 256.0)) % 8;
    converted_ss << std::setw(1) << (last_digit == 4 ? "+" : std::to_string(last_digit));
    return converted_ss.str();
}

std::string convert_to_256th(double price){
    std::stringstream converted_ss;
    converted_ss << std::round(price * 256.0) << "/256";
    return converted_ss.str();
}

#endif