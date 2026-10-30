#pragma once
#include <string>

// The game thread publishes a request; Android's UI thread completes it.
void pes_osk_begin(const std::u16string &initial);
bool pes_osk_take_result(std::u16string &text, bool &accepted);
void pes_osk_reset();
