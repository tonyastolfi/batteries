//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_VERBOSE_HPP
#define BATTERIES_STATE_MACHINE_MODEL_VERBOSE_HPP

#include <iostream>

#if 0
#define BATT_STATE_MACHINE_VERBOSE() 

for (bool b_VerBOSe_vAR = true; b_VerBOSe_vAR; std::cout << std::endl, b_VerBOSe_vAR = false)                \
    std::cout
#else
#define BATT_STATE_MACHINE_VERBOSE()                                                                         \
    for (; false;)                                                                                           \
    std::cout
#endif

#endif  // BATTERIES_STATE_MACHINE_MODEL_VERBOSE_HPP
