#include "jobsystem.hpp"

#include <thread>
#include <chrono>
#include <iostream>

int main()
{
   JobSystem jobSystem;

   JobHandle root = jobSystem.create([] {});

   JobHandle handle1 = jobSystem.create([] {std::this_thread::sleep_for(std::chrono::seconds(1));});
   jobSystem.schedule(handle1);

   JobHandle handle2 = jobSystem.create([] {std::cout << "INSTANT HELLO\n";}, root);
   jobSystem.schedule(handle2);

   JobHandle handle3 = jobSystem.create([] {std::cout << "WAITING HELLO\n";}, root);
   jobSystem.schedule(handle3, handle1);

   JobHandle handle4 = jobSystem.create([] {std::cout << "INSTANT HELLO\n";}, root);
   jobSystem.schedule(handle4);

   jobSystem.schedule(root);

   jobSystem.wait(root);

   std::cout << "AFTER ALL HELLO\n";

   // Try to depend on multiple tasks
   root = jobSystem.create([] {});

   // Task D, depend on task A, B and C
   // To do that, create root, and assign root as a parent of A, B and C
   JobHandle taskA = jobSystem.create([] {std::cout << "TASK A!!!\n";}, root);
   JobHandle taskB = jobSystem.create([] {std::cout << "TASK B!!!\n";}, root);
   JobHandle taskC = jobSystem.create([] {std::cout << "TASK C!!!\n";}, root);

   JobHandle taskD = jobSystem.create([] {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::cout << "TASK D!!!\n";
   }, root);

   jobSystem.schedule(taskD, root);
   jobSystem.schedule(taskA);
   jobSystem.schedule(taskB);
   jobSystem.schedule(taskC);
   jobSystem.schedule(root);

   jobSystem.waitAll();
}