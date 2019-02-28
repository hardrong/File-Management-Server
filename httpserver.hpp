#pragma once 

#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <unordered_map>
#include "utils.hpp"
#include "threadpool.hpp"

#define MAX_LISTEN 5
#define MAX_THREAD_NUM 3


class Sock 
{
  private:
    int _fd;
  public:
    Sock(int fd = -1)
      :_fd(fd)
    {
               
    }

    ~Sock()
    {


    }
    bool Socket()
    {
       _fd = socket(AF_INET,SOCK_STREAM,0);
      if(_fd < 0)
      {
        std::cout << "socket error" << std::endl;
        return false;
      }
        int opt = 1;
        setsockopt(_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
          
      return true;
    }
    bool Bind(const uint16_t port)
    {
      struct sockaddr_in local;
      bzero(&local,sizeof(local));
      local.sin_family = AF_INET;
      local.sin_addr.s_addr = INADDR_ANY;
      local.sin_port = htons(port);
      
      int ret = bind(_fd,(struct sockaddr *)&local,sizeof(local));
      if(ret < 0)
      {
          std::cout << "socket bind error" << std::endl;
          return false;
      }
              return true;
    }
    bool Listen(int num)
    {
        int ret = listen(_fd,num);
        if(ret < 0)
        {
          std::cout << " listen error" << std::endl;
          close(_fd);
          return false;
        }
        std::cout << "Listen sucess" << std::endl;
          return true;

    }
    int Accept(sockaddr_in * pclient = NULL ,socklen_t *plen = NULL)
    {
      int new_sock = accept(_fd,(struct sockaddr*)pclient,plen);
      if(new_sock < 0)
      {
        std::cout << "accept error" << std::endl;
        return -1;
      }

      std::cout << "New Client Connect : [ip:]" << inet_ntoa(pclient->sin_addr)\
       
      << " [port]" << ntohs(pclient->sin_port) << std::endl;
  
      return new_sock;
	}
   bool Close()
   {
     close(_fd);
     return true;
   }
};


class HttpServer
{
  private:
    ThreadPool *_pt;
    Sock _listen_sock;
    uint16_t _port;
  public:
    HttpServer(const uint16_t port):_port(port)
    {
      
    }
    bool HttpServerInit(size_t num = MAX_THREAD_NUM)
    {
      _pt = new ThreadPool(num);
       _pt->ThreadPoolInit();

       return _listen_sock.Socket() && _listen_sock.Bind(_port) &&  _listen_sock.Listen(num);
    }
   void Start()
   {
     while(1)
     {
        sockaddr_in client;
        socklen_t len = sizeof(client);
        int newsock = _listen_sock.Accept(&client,&len);
        if(newsock < 0)
        {
          std::cout << "accept failue" <<std::endl;
          continue;
        }

     Service(newsock);
     }
     _listen_sock.Close();
     return ;
   }
   void Service(int newsock)
   {
      HttpTask t(newsock,Handler);
      _pt->PushTask(t);
   }
   static bool Handler(int newsock)
   {
        RequestInfo info;
        HttpRequest req(newsock);
       HttpResponse rsp(newsock);
       

      //  接受http头部
 if(req.RecvHttpHeader(info) == false)
  {   
      goto out; 
  }
 
        //解析请求头
if(req.ParseHttpHeader(info) == false)
 {
        goto out;
     
 }
        //判断请求类型是否为CGI请求
       
if(info.RequestIsCGI(info))
 {
    //若当前请求类型是CGI请求，则执行CGI请求 
       rsp.CGIHandler(info);
       
 }
 else
 {
        //若当前请求不是CGI请求
        //则执行目录列表、文件下载响应
    rsp.FileHandler(info);
      
 }
     
       close(newsock);
        return true;
    out:
        rsp.ErrHandler(info);
        close(newsock);
        return true;
    }
};


