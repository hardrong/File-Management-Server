#include "utils.hpp"
#include "httpserver.hpp"
#include <signal.h>
int  main(int argc,char* argv[])
{
  // 要忽略掉
   signal(SIGPIPE,SIG_IGN);// 重要，当客户端关闭链接时，服务器再发送数据的时候，给服务器发送SIGPIPE信号，默认终止进程
   if(argc != 2) 
   {
    std:: cerr << "arguement error!" << std::endl;
      exit(1);
   
   }
        
   HttpServer* pserver = new HttpServer(atoi(argv[1])); 
   pserver->HttpServerInit();   
   pserver->Start();
   return 0;
}

