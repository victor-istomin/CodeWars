# CodeWars
Russian AI Cup 2017 solution

This is mostly a sport-programming quality code which was written in a rush of competition. 
However, it may be interesting as an example of solving tasks in case of limited time resources. 

Due to folder structure restrictions of RAIC, strategy code is implemeted within RAIC SDK folder. 

There is short list of places of my code to start digging from: 

 * [MyStrategy.cpp](cpp-cgdk/MyStrategy.cpp)   - main file of strategy implmementaton
 * [goalManager.cpp](cpp-cgdk/goalManager.cpp) - core of bot logic
 * [goal.h](cpp-cgdk/goal.h)                   - goals base implementation. Note: private virtuals are used [https://isocpp.org/wiki/faq/strange-inheritance#private-virtuals]
 
