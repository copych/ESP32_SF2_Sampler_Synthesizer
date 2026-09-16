/*
Simple Timer

MIT License

Copyright (c) 2025 ndenki

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include "SimpleTimer.h"

// Constructor implementation with initialCount parameter
SimpleTimer::SimpleTimer(unsigned long intervalMillis, int initialCount)
    : previousMillis(millis()), interval(intervalMillis), count(initialCount) {}

// Check if interval has passed since last run
bool SimpleTimer::shouldRun(int* runCount) {
  unsigned long currentMillis = millis();
  unsigned long elapsed = currentMillis - previousMillis;

  if (elapsed >= interval) {
    unsigned long runs = elapsed / interval;
    previousMillis += runs * interval;
    count += runs;

    if (runCount) {
      *runCount = runs;
    }

    return true;
  }

  return false;
}

// Set a new interval in milliseconds
void SimpleTimer::setInterval(unsigned long intervalMillis) {
  interval = intervalMillis;
}

// Reset the timer and the count
void SimpleTimer::reset() {
  previousMillis = millis();
  count = 0;
}

// Get how many times the timer has run
unsigned long SimpleTimer::getCount() const {
  return count;
}

// Get the current toggle state (true if count is odd, false if even)
// Initial value is false (count = 0)
// If you want the opposite behavior, invert it with !getToggle()
bool SimpleTimer::getToggle() const {
  return (count & 1);  // bitwise operation is efficient for modulo 2
}

// setCount implementation
void SimpleTimer::setCount(int newCount) {
    count = newCount;
}
