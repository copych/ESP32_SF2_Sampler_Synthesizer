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
#ifndef SIMPLETIMER_H
#define SIMPLETIMER_H

#include <Arduino.h>

class SimpleTimer
{
public:
    // Constructor with interval and optional initial count (default 0)
    SimpleTimer(unsigned long intervalMillis = 1000, int initialCount = 0);

    // Check if the timer should run based on the elapsed time
    // Optionally returns how many times the interval has passed
    bool shouldRun(int *runCount = nullptr);

    // Set a new interval in milliseconds
    void setInterval(unsigned long intervalMillis);

    // Reset the timer and the internal count
    void reset();

    // Get how many times the timer has run
    unsigned long getCount() const;

    // Get the current toggle state (true if count is odd, false if even)
    // Initial value is false (count = 0)
    // If you want the opposite behavior, invert it with !getToggle()
    bool getToggle() const;
    
    // Set the internal count value.
    // This allows externally controlling the timer's count, useful for initializing or resetting the timer state.
    // Parameters:
    //   newCount - The new count value to assign.
    void setCount(int newCount);

private:
    unsigned long previousMillis;
    unsigned long interval;
    unsigned long count = 0;
};

#endif
