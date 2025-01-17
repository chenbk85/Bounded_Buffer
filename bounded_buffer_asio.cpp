////=====================================================================================================================================================
// Name        : bounded_buffer_asio.cpp
// Author      : l33pf
// Version     : 1
// Copyright   : Your copyright notice
// Description : Bounded Buffer, Producer-Consumer in C++11
//				 This version of the Bounded Buffer uses Boost's ASIO Library
//				 and achieves concurrency through using a Thread Pool. 
//
//
// Misc:         Eclipse setup -
// Header Link:  Project->Properties->Settings->GCC C++ Compiler->Includes->/usr/include/boost
// C++11 Flag:   Project->Properties->Settings->GCC C++ Compiler->Misc->add: -std=c++11
// Linker:       Project->Properties->Settings->GCC C++ Linker->Libraries->add: boost_filesystem, boost_thread, boost_system, boost_iostreams, pthread
//               GCC setup -
// Command Setup: g++ -std=c++11 bounded_buffer_asio.cpp -o bounded_buffer -lboost_system -lboost_thread -lboost_iostreams -lpthread (Compile)
//                ./bounded_buffer_asio (Run)
//======================================================================================================================================================

/* BOOST LIBS */
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

/* CPP LIBS */
#include <iostream>
#include <typeinfo>
#include <string>
#include <vector>
#include <cstdio>

/*This overall size of the Buffer */
#define size 100
/*Keep an external record of the number of items in the buffer */
int count = 0;
/*Buffer used by the Producer and the Consumer */
std::vector<std::string> data_buffer;
/*Mutex for Threading purposes */
boost::mutex data_ready_mu;
/*Conditon variable instance for Threading purposes */
boost::condition_variable data_ready_con;

//Returns a string with random characters. It
//Is up to the user to define the length of the string.
//Currently the function is formatted for C++11 usage,
//Though can be changed to other versions.
inline std::string random_string(size_t length)
{
	auto randchar = []() -> char
			{
			  const char alph_set[] =
					  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					  "abcdefghijklmnopqrstuvwxyz";
			  const size_t max_index = (sizeof(alph_set) - 1);
			  return alph_set[rand() % max_index];
			};
	std::string rand_str(length,0);
	std::generate_n(rand_str.begin(),length,randchar);

	return rand_str;
}

//Producer function
//Fills up the buffer with random generated strings
//Awaits for the producer to start taking off items
//Current work time set is 3 secs.
inline void producer()
{
	std::string item;
	boost::posix_time::seconds workTime(3);
	boost::unique_lock<boost::mutex> lock(data_ready_mu);

	while(true)
	{
		item = random_string(5);

		if(count == size)
		{
			/* Block appropriately for three seconds */
			boost::this_thread::sleep(workTime);
			//Lock the Buffer in order to start populating items
			data_ready_con.wait(lock);
		}

		/* Push item onto the Buffer */
		data_buffer.push_back(item);
		count++;

		if(count == 1)
		{
			/* Notify the Consumer thread that items need to be taken off */
			data_ready_con.notify_all();
		}
	}
}

//Consumer function
//Takes items off the buffer and prints them to console
//Awaits the Producer to start filling up the buffer
inline void consumer()
{
	std::string item;
	boost::posix_time::seconds workTime(3);
	boost::unique_lock<boost::mutex> lock(data_ready_mu);

	while(true)
	{
		if(count == 0)
		{
			/* Block until the buffer starts to be filled again */
			boost::this_thread::sleep(workTime);
			data_ready_con.wait(lock);
		}

		/*Get the most recent item off the buffer */
		item = data_buffer.back();
		/*Take the element off the buffer */
		data_buffer.pop_back();
		/*Decrement*/
		count--;

		/*Is the buffer full */
		if(count == size - 1)
		{
			/* Notify the Producer thread that items need to be put on */
			data_ready_con.notify_all();
		}

		/*Print the Consumed Item out to the console */
		std::cout << "This is the item consumed: " << item << std::endl;
	}
}

int main(int argc, char **argv[])
{
  
  /* Create an I/O Service */
	boost::asio::io_service ioservice;
	/* Inititate a new Thread pool */
	boost::thread_group threadpool;

  /* Assign a worker object to the I/O Service */
	boost::asio::io_service::work work(ioservice);

	/*If the buffer size is evaluated to empty */
	if(data_buffer.empty())
	{
		/* Then reassign and make sure the producer and consumer know */
		count = 0;
	} /* Otherwise if it is full */
	else if(data_buffer.capacity() == size)
	{
		/* then Reassign to notify the producer and consumer*/
		count = size;
	}

	/* Add two threads to the thread pool */
	threadpool.create_thread(boost::bind(&boost::asio::io_service::run,&ioservice));
	threadpool.create_thread(boost::bind(&boost::asio::io_service::run,&ioservice));

	/* Assign the tasks to the Thread pool */
	ioservice.post(boost::bind(producer));
	ioservice.post(boost::bind(consumer));

	/* Stop i/o service processing loop, Possible bug in some jobs may go unfinished */
	//ioservice.stop();

	/* Destruct work object, Allows all jobs to finish */
	work.~work();

	/* Join all threads in the thread pool together once completed */
	threadpool.join_all();

	return 0;
}
