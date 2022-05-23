#pragma once

// Unknown message type
constexpr int UNKNOWN =               0b00000000;
// Winemaker wine exposition broadcast message.
constexpr int WINEMAKER_BROADCAST =   0b00000001;
// Winemaker safehouse aquisition REQ message.
constexpr int WINEMAKER_ACQUIRE_REQ = 0b00000010;
// Winemaker safehouse aquisition ACK message.
constexpr int WINEMAKER_ACQUIRE_ACK = 0b00000100;
// Student empty safehouse broadcast message.
constexpr int STUDENT_BROADCAST =     0b00001000;
// Student wine acquisition REQ message.
constexpr int STUDENT_ACQUIRE_REQ =   0b00010000;
// Student wine acquisition ACK message.
constexpr int STUDENT_ACQUIRE_ACK =   0b00100000;