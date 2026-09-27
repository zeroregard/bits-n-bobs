#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#undef GetObject

#include <atomic>
#include <cctype>
#include <mutex>
#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <thread>

using namespace std::literals;

namespace logger = SKSE::log;
