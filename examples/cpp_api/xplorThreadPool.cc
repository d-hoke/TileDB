
/*
 g++ -I. \
 -I/mnt/tiledbdrv/home/dev1/dev/tiledbubuntu18.04/gh.d-hoke.tiledb.git/ \
 -I/mnt/tiledbdrv/home/dev1/dev/tiledbubuntu18.04/gh.d-hoke.tiledb.git/build/externals/src/ep_spdlog/include \
 main.cc thread_pool.cc status.cc -lpthread 2>&1 | less
 */
//

#include <atomic>
#include <string>
#include <map>
#include <chrono>
#include <exception>

#include <signal.h>

#include "common/thread_pool.h"

using namespace tiledb::common;
//using namespace std::chrono_literals;

#define UNUSED(x) (void)(x)

int test1(int argc, char **argv)
{
   UNUSED(argc); UNUSED(argv);
   ThreadPool pool;
//   REQUIRE(pool.init().ok()); //default to ? threads

   return 0 ;
}

int test2(int argc, char **argv)
{
   UNUSED(argc); UNUSED(argv);
   ThreadPool pool;
//   REQUIRE(pool.init(4).ok());

   std::atomic<int> result (0);
   auto task1 = pool.execute([&]() 
		{
		   ++result;
		   return Status::Ok();
		}
		);
   
   return 0;
}

#define ASSERT( x ) if(!(x)) throw std::string(#x);

int test3(int argc, char **argv)
{
   UNUSED(argc); UNUSED(argv);
   //exploring loop used in Writer::write_all_tiles()
   ThreadPool pool;
   ASSERT(pool.init(7).ok());
   
   std::unordered_map<std::string, uint64_t> mymap;
   int nmapitems = 1000;
//   int nmapitems = 100;
//   int nmapitems = 50;
   for(auto i = 0 ; i < nmapitems; ++i)
   {
      std::string astr = std::to_string(i);
      mymap[astr] = i;
   }
//   std::atomic<int> good = 0;
//   std::atomic<int> bad = 0;
   std::atomic_int good(0);
   std::atomic_int bad(0);
   std::vector<ThreadPool::Task> tasks;
   tasks.reserve(nmapitems);
   int maxiters = 20000;
   //int maxiters = 1000;
   //int maxiters = 5000;
   for(int itrs = 0 ; itrs < maxiters; ++itrs)
   {
      tasks.clear();
   for (auto& it : mymap) //seems to do the 'right thing', can it be trusted per language standards?
//   for (auto it : mymap) //fails
   {
      auto &skey = it.first;
      tasks.push_back( pool.execute([&,skey]() {
//	 std::this_thread::sleep_for(3s);
//	 printf("%s, %lu\n", it.first.c_str(), it.second);
	 auto &itm = mymap[skey];
	 if(&itm != &it.second)
	 {
	    ++bad;
	    printf("ERROR %lu, addresses don't match\n", itm);
	 }
	 else 
	 {
	    ++good;
	 }
	 
	 return Status::Ok();
	 
      })
      );
				   
   }
   
   //std::this_thread::sleep_for(5s);
   pool.wait_all_status(tasks);
   if(itrs % 1000 == 0) printf("itrs %d of %d\n",itrs, maxiters);
   }
   
   printf("good %d, bad %d\n", good.load(), bad.load());
   
   return 0;
}

int test4(int argc, char **argv)
{
   UNUSED(argc); UNUSED(argv);

   signal(SIGUSR1, SIG_IGN);
   printf("running on pid %x, %d\n", getpid(), getpid());
   //exploring loop used in Writer::write_all_tiles()
   ThreadPool pool;
//   ASSERT(pool.init(7).ok());
   ASSERT(pool.init(96).ok());
//   ASSERT(pool.init(1000).ok());
//   ASSERT(pool.init(500).ok());
   
   std::unordered_map<std::string, uint64_t> mymap;
//   int nmapitems = 2; //think one task won't make it to .execute/.wait point...
//   int nmapitems = 1000;
//   int nmapitems = 2000;
   int nmapitems = 10000;
//   int nmapitems = 50000;
//   int nmapitems = 100;
//   int nmapitems = 50;
   for(auto i = 0 ; i < nmapitems; ++i)
   {
      std::string astr = std::to_string(i);
      mymap[astr] = i;
   }
//   std::atomic<int> good = 0;
//   std::atomic<int> bad = 0;
   std::atomic_int good(0);
   std::atomic_int bad(0);
   std::vector<ThreadPool::Task> tasks;
   tasks.reserve(nmapitems);
   //int maxiters = 1;
   //int maxiters = 20000;
   //int maxiters = 1000;
   int maxiters = 300;
   //int maxiters = 5000;
   
   time_t st1, et1;
   time(&st1);
   for(int itrs = 0 ; itrs < maxiters; ++itrs)
   {
      tasks.clear();
   for (auto& it : mymap) //seems to do the 'right thing', can it be trusted per language standards?
//   for (auto it : mymap) //fails
   {
      UNUSED(it);
      //auto &skey = it.first;
      tasks.push_back( pool.execute([&]() {
	 //auto waitival = 5s;
//	 auto mypid = getpid();
//	 printf("thread %d  pausing...\n", mypid);
	 //std::this_thread::sleep_for(waitival);
	 
	 //busy pause...
	 time_t t1, t2;
	 time(&t1);
#if 0
	 uint64_t cntr = 0;
	 do
	   {
	      ++cntr;
	      time(&t2);
	   }
//	 while(difftime(t2, t1) < 5);
	 while(difftime(t2, t1) < .01);
#else
	 time(&t2);
#endif
	 
	 
//	 printf("thread %d done pausing, difftime %f.\n",mypid, difftime(t2,t1));
	 
	 //pausing to allow outside world to signal, seeing if spurious wakeup generated...
	 
	 return Status::Ok();
	 
      })
      );
				   
   }

//   printf("thread %x waiting 10...\n", getpid());
//   sleep(10);
   //Have we managed to produce lost wakeup()...
   printf("now, .wait_all_status()\n");
   //std::this_thread::sleep_for(5s);
   auto results = pool.wait_all_status(tasks);
      int cntok=0, cntnotok = 0;
      for(auto &vitr : results) {
	 if( vitr.ok())
	   ++cntok;//,
	   //0; //printf("wait_all ok.\n");
	 else
	   ++cntnotok,
	   printf("wait_all ! OK\n");
	 
      }
      printf("ok %d, notok %d\n", cntok, cntnotok);
   time(&et1);
      
   printf("difftime %f\n", difftime(st1,et1));
      
   if(itrs % 1000 == 0) printf("itrs %d of %d\n",itrs, maxiters);
   }
   
   printf("good %d, bad %d\n", good.load(), bad.load());
   
   return 0;
} //test4()

int main(int argc, char *argv[])
{
   int retstat = 0;

//  test3(argc, argv);

   test4(argc, argv);//test threads with pause seeing if spurious wakeup can occur with outside influence...

   return retstat;
}
