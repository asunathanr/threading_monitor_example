#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <condition_variable>
#include <iostream>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

/**
   FILE: main.cpp
   AUTHOR: Nathan Robertson
   Exploration of monitors as used in multi-threaded programming.
   A monitor can be defined as a paired condition variable and mutex 
   
   To build this code run the command cmake .; make
   Run this code with ./monitor_test
   Since the -pthread flag is used with the compiler the target system needs to have the pthread library for this code to compile.
   
   The code in this project demonstrates how cout can become interleaved across multiple threads, and how monitors can fix this issue.
   Three seperate examples are performed. 
   1. Single threaded example -- demonstrates expected output in a single threaded application
   2. Unmonitored multi threaded operation -- demonstrates potential race conditions associated that can occur without the use of a monitor.
   3. Monitored multi threaded operation -- demonstrates correct result obtained by using a monitor for a multithreaded application.
 */


std::string rock_text;
std::string hamlet_text;


// Shows what text should look like in ordinary single threaded app.
void singleThreadTest();

// Displays interleaving that occurs when two threads run at once.
void raceTest();

// Resolves interleaving and displays results identical to the single thread.
void monitorTest();

// Puts a slight pause between characters such that if another thread runs this function in parallel the characters will interleave.
void displayTextSlowly(const std::string& text, const std::chrono::milliseconds delay);

// Reads text to be placed in strings
std::string readFile(const char* fileName);


// Driver thread function for monitor test
int main() {
  rock_text = readFile("i_wanna_rock.txt");
  hamlet_text = readFile("hamlet.txt");
  singleThreadTest();
  raceTest();
  monitorTest();
  // Exit the entire process -- this ensures all threads will shut down correctly.
  exit(0);
  return 0;
}


void singleThreadTest() {
  std::cout << "Displaying texts in single thread:\n"
	    << "I Wanna Rock by Twisted Sister: \n" << rock_text << "\n"
	    << "Hamlet Act III Scene I: \n" << hamlet_text << "\n\n";
}


// Multithreading globals
const std::chrono::milliseconds TEXT_DELAY(3);
const std::chrono::milliseconds MAIN_WAIT(1000);

bool raceTestsFinished = false;
std::mutex raceMutex;
std::condition_variable raceTestVar;

// Note that even though this is purposeful race code in order to ensure that monitorTest() runs uninterupted another monitor must keep this function from returning
// until the threads it creates have finished running.
void raceTest() {
  std::cout << "Displaying texts with race conditions: \n";
  std::thread([](){
      std::cout << "I Wanna Rock by Twisted Sister: \n";
      displayTextSlowly(rock_text, TEXT_DELAY);
    }).detach();
  std::thread([](){
      std::cout << "Hamlet Act III Scene I: \n";
      displayTextSlowly(hamlet_text, TEXT_DELAY);
      raceTestsFinished = true;
      raceTestVar.notify_all();
    }).detach();
  
  std::unique_lock<std::mutex> lock(raceMutex);
  // See the monitorTest function for explanation of the while loop around the call to wait
  while (!raceTestsFinished) {
    raceTestVar.wait(lock, [](){ return raceTestsFinished; });
  }
  // Wait a little more time just to be safe.
  std::this_thread::sleep_for(MAIN_WAIT);
  std::cout << "\n";
  lock.unlock();
}

bool finishedFirstText = false;
std::mutex printMutex;
std::condition_variable printCondition;


void monitorTest() {
  std::cout << "Displaying texts in correct order with a monitor: \n";

  // This thread is allowed to print immediately, but it must communicate that it is done with shared printing objects to any waiting threads
  std::thread([](){
      std::cout << "I Wanna Rock by Twisted Sister: \n";
      displayTextSlowly(rock_text, TEXT_DELAY);
      std::putchar('\n');
      // We are done using global print objects in this thread; other threads will use the below flag to verify this.
      finishedFirstText = true;
      // Wakeup other threads that are waiting to print.
      printCondition.notify_all();
    }).detach();

  // This thread must use a monitor to wait until the thread above gives it the all clear. The text it prints should be below the above threads text.
  std::thread([](){
      std::unique_lock<std::mutex> lock(printMutex);
      // We need the while loop around the wait method call to make sure a spurious wakeup didn't occur.
      // https://hazelcast.com/blog/spurious-wakeups-are-real-4/
      while(!finishedFirstText) {
	printCondition.wait(lock, []() { return finishedFirstText; });
      }
      std::cout << "Hamlet Act III Scene I: \n";
      displayTextSlowly(hamlet_text, TEXT_DELAY);
      lock.unlock();
    }).detach();

  std::this_thread::sleep_for(std::chrono::milliseconds(MAIN_WAIT + (rock_text.size() + hamlet_text.size()) * TEXT_DELAY));
  std::cout << "\n";
}


void displayTextSlowly(const std::string& text, const std::chrono::milliseconds delay) {
  for (char c : text) {
    std::putchar(c);
    std::this_thread::sleep_for(delay);
  }
}


std::string readFile(const char* fileName) {
  std::string data;
  std::string line;
  std::ifstream inputStream(fileName);
  if (inputStream) {
    while (inputStream) {
       std::getline(inputStream, line);
       data += line + '\n';
    }
  }
  else {
    std::cerr << "Fatal Error: Could not open input file\n";
    exit(1);
  }
  inputStream.close();
  return data;
}
