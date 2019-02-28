#pragma once 
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sstream>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <strings.h>
#include <time.h>
#include <vector>
#include <unistd.h>
#include <unordered_map>
#include <dirent.h>
#define MAX_HTTPHDR 4096
#define WWWROOT  "www"
#define MAX_BUFF 4096

std::unordered_map<std::string,std::string> g_mime_type =
{
  {"txt",  "application/octet-stream"},
  {"html","text/html"},
  {"htm", "text/html"},
  { "jpg", "image/jpeg" },
  {"mp3", "audio/mpeg"},
  {"mpeg", "video/mpeg"},
  {"unknow", "application/octet-stream"}
};
std::unordered_map<std::string,std::string> g_err_desc = {
  { "200","OK"},        //正常处理
  {"206"," partial Content"}, // 分块传输
  {"302",  "Found"},   //临时重定向
  {"400", "Bad Request"}, 
  {"403", "Forbidden"},   
  {"404", "Not Found"},    
  {"405", "Method Not Allowed"},
  {"413", "Request Entity Too Large"},
  {"500", "Internal Server Error"}
};
class Utils
{
  private:
    std::string _file_name;
    std::string _file_id;
    std::string _first_boundry;
    std::string _middle_boundry;
    std::string _last_boundry;
    std::string _content_len;
    bool _is_store_file;
  public:
    bool InitUploadInfo();//初始化上传文件的信息，长度
    bool ProcessUpload();//完成文件上传的存储功能
  public:
    static int Split(std::string &src,const std::string &seg,std::vector<std::string>&list)
    {
      int num = 0,idx = 0;
      size_t pos = 0;
      while(idx <= src.length())
      {
        pos = src.find(seg,idx);
        if(pos == std::string::npos)
        {
          break;
        }
        list.push_back(src.substr(idx,pos - idx));
        num++;
        idx = pos+seg.length();
      }
      return num;
    }     
    static std::string GetErrDesc(std::string &code)
    {
      auto it = g_err_desc.find(code);
      if(it == g_err_desc.end())
      {
        return "Unknow Error";
      }
      return it->second;
    }
    static void  TimeToGMT(time_t t, std::string &gmt)
    {
      struct tm* mt = gmtime(&t);
      char tmp[128] = {0};
      strftime(tmp,127,"",mt);
      int len = strftime(tmp,127,"%a,%d,%b,%Y,%H:%M:%S GMT",mt );
      gmt.assign(tmp,len);

    }
    static void DigitToStr(int64_t num,std::string &str)
    {
      std::stringstream ss;
      ss << num;
      str = ss.str();

    }
    static int64_t StrToDigit(const std::string &str)
    {
      int64_t num;
      std::stringstream ss;
      ss << str;
      ss >> num;
      return num;
    }
    static void MakeETag(int64_t size,int64_t ino,int64_t mtime,std::string &etag)
    {
      //"ino-size-mtime";
      std::stringstream ss;
      ss << "\"" << std::hex << ino <<"-" << std::hex<<size << "-" << std::hex<<mtime << "\"";
      etag = ss.str();

    }
    static void GetMime(const std::string &file ,std::string &mime)
    {
      size_t pos;
      pos = file.find_last_of(".");
      if(pos == std::string::npos)
      {
        mime = g_mime_type["unknow"];
        return;
      }
      std::string suffix = file.substr(pos+1);
      auto it = g_mime_type.find(suffix);
      if(it == g_mime_type.end())
      {
        mime = g_mime_type["unknow"];
        return ;
      }
      else 
      {
        mime = it->second;
      }
    }
};
class RequestInfo 
{
  public:
    std::string _method;//请求方法
    std::string _version; // 协议方法
    std::string _path_info; // 资源路径
    std::string _path_phys ; // 实际路径
    std::string _query_string; //查询字符串
    std::unordered_map<std::string,std::string> _hdr_list; //整个头部信息的键值对
    struct stat _st;
  public:
    std::string _err_code;
    void SetError(const std::string &code)
    {
      _err_code = code;
    }
    bool RequestIsCGI(RequestInfo &info)
    {
      if((_method == "GET" && !_query_string.empty()) ||
          (_method == "POST"))
        return true;

      return false;
    }

};
class HttpRequest 
{
  int _cli_sock;
  RequestInfo _req_info;
  std::string _http_header;
  public:
  HttpRequest(int cli_sock):_cli_sock(cli_sock)
  {

  }
  bool RecvHttpHeader(RequestInfo &info)
  {
    char tmp[MAX_HTTPHDR];
    int ret = recv(_cli_sock,tmp,MAX_HTTPHDR,MSG_PEEK);//从缓冲区接受数据，但是并不从缓冲区删除数据--MSG_PEEK
    int hdr_len;
    while(1)
    {
      if(ret <= 0)
      {
        if(errno == EINTR || errno == EAGAIN) //EINTR --信号打断，再读一次
          continue;

        info.SetError("500");
        return false;
      }
      char *ptr = strstr(tmp,"\r\n\r\n");
      if((ptr == NULL) && ret == MAX_HTTPHDR)
      {
        info.SetError("413");
        return false;
      }
      else if((ptr == NULL) && ret < MAX_HTTPHDR)
      {
        usleep(1000);
        continue;
      }
      hdr_len = ptr -tmp;
      _http_header.assign(tmp,hdr_len); //从tmp中截取hdr_len；
      recv(_cli_sock,tmp,hdr_len+4,0); //将头部移除 
      break;
    }
    std::cout << _http_header<<std::endl;  
    return true;
  }

  bool ParseFirstLine(std::string &line, RequestInfo &info)
  {
    std::vector<std::string> line_list;
    if(Utils::Split(line," ", line_list) != 2)
    {
      info._err_code = "400";
      return false;
    }
    std::string url;
    info._method = line_list[0];
    url = line_list[1];
    info._version = "HTTP/1.1";

    if(info._method != "GET" && info._method != "POST" && info._method != "HEAD")                
    {                                                                                            
      info._err_code = "405";                                                                   
      return false;                                                                             
    }                                                                                            
  if(info._version != "HTTP/0.9" && info._version != "HTTP/1.0" && info._version != "HTTP/1.1" ) 
  {
      info._err_code = "400";
      return false;
   }
  //url : www/../..?key=val&ley=val 
  size_t pos;
  pos = url.find("?");
  if(pos == std::string::npos)
  {
    info._path_info = url;
  }
  else 
  {
   info._path_info = url.substr(0,pos);
   info._query_string = url.substr(pos+1);
  }
  return PathIslegal(info);
 }
  bool PathIslegal(RequestInfo &info)
  {
    std::string file = WWWROOT + info._path_info;
    if(stat(file.c_str(), &info._st) <0 )
    {
      info._err_code = "404";
      return false;
    }
    char tmp[4096] = {0};
    realpath(file.c_str(), tmp);
    info._path_phys = tmp;
    if(info._path_phys.find(WWWROOT) == std::string::npos)
    {
      info._err_code = "403";
      return false;
    }
    return true;
  }
  bool ParseHttpHeader(RequestInfo &info)
  {
    std::vector<std::string> hdr_list;
    Utils::Split(_http_header,"\r\n",hdr_list);

    if(ParseFirstLine(hdr_list[0],info) == false)
    {
      return false;
    }
    for(int i = 1; i< hdr_list.size();i++)
    {
      size_t pos = hdr_list[i].find(": ");
      info._hdr_list[hdr_list[i].substr(0,pos)] = hdr_list[i].substr(pos+2);
    }

    return true;
  }
};
class HttpResponse 
{

  int _cli_sock;
  std::string _etag;  //源文件是否修改过    inode -fsize -mtime
  std::string _mtime;//最后一次修改时间
  std::string _date;
  std::string _fsize;
  std::string _mime;

  public:
  HttpResponse(int cli_sock):_cli_sock(cli_sock)
  {

  }
  bool ProcessFile(RequestInfo &info)
  {

    std::string rsp_header;
    rsp_header = info._version + " " + "200 OK \r\n";
    rsp_header += "Content-Type: application/octet-stream\r\n";
    rsp_header += "Connection: close\r\n";
    rsp_header += "Connection-Length" + _fsize + "\r\n"; 
    rsp_header += "ETag: " + _etag + "\r\n";
    rsp_header += "Last-Modified: " + _mtime + "\r\n";
    rsp_header += "Date: " + _date + "\r\n\r\n";
    SendData(rsp_header);

    int fd = open(info._path_phys.c_str(),O_RDONLY);
    if(fd < 0)
    {
      info._err_code = "400";
      ErrHandler(info);
      return false;
    }
    int rlen = 0;
    char tmp[MAX_BUFF];
    while((rlen = read(fd,tmp,MAX_BUFF)) > 0)
    {
      send(_cli_sock,tmp,rlen,0);     // 内容如果本身都是0就不能用string发送数据

    }
    close(fd);
    return true;
  }
  bool InitResponse(RequestInfo &info)
  {
    //Last-Modified;
    Utils::TimeToGMT(info._st.st_mtime,_mtime);
    //ETag;
    Utils::MakeETag(info._st.st_size,info._st.st_ino,info._st.st_mtime,_etag);
    //Date;
    time_t t = time(NULL);
    Utils::TimeToGMT(t,_date);
    Utils::DigitToStr(info._st.st_size,_fsize);
    Utils::GetMime(info._path_phys,_mime);
    //文件大小
    return true;
  }
  bool SendData(const std::string &buf)
  {
    if(send(_cli_sock,buf.c_str(),buf.length(),0) < 0)
      return false; 
    return true;

  }
  bool SendCData(const std::string &buf)
  {
    if(buf.empty())
    {
      return  SendData("0\r\n\r\n");
    }

    std::stringstream ss;
    ss <<std::hex <<buf.length() << "\r\n";
    SendData(ss.str());
    ss.clear();
    SendData(buf);
    SendData("\r\n");
    return true;
  }
  bool ProcessList(RequestInfo &info)
  {
    //组织头部；
    //首行
    //Content-Type：text/html\r\n
    //ETag:\r\n
    //Date:\r\n
    // Connection:close\r\n
    //正文：
    std::string rsp_header;
    rsp_header = info._version + " " + "200 OK \r\n";
    rsp_header += "Content-Type: text/html\r\n";
    rsp_header += "Connection: close\r\n";
    if(info._version == "HTTP/1.1")
    {
      rsp_header += "Transfer-Encoding: chunked\r\n";
    }
    rsp_header += "ETag: " + _etag + "\r\n";
    rsp_header += "Last-Modified: " + _mtime + "\r\n";
    rsp_header += "Date: " + _date + "\r\n\r\n";
    SendData(rsp_header);

    std::string rsp_body;
    rsp_body = "<html><head>";
    rsp_body += "<title>hardrong  "+info._path_info + "</title>";
    rsp_body += "<meta charset='UTF-8'>";
    rsp_body += "</head><body>";
    rsp_body += "<h1>hardrong " + info._path_info + "</h1>";
    rsp_body += "<form action='/upload' method='POST' ";
    rsp_body += "enctype='multipart/form-data'>";
    rsp_body += "<input type='file' name='FileUpLoad' />";
    rsp_body += "<input type='submit' value='上传' />";
    rsp_body += "</form>";
    rsp_body +=  "<hr /><ol>";

    SendCData(rsp_body);

    struct dirent **p_dirent = NULL;
    //获取目录下的每一个文件，组织处html信息，chunke传输
    int num = scandir(info._path_phys.c_str(),&p_dirent,0,alphasort);
    for(int i=0; i < num; i++)
    {
      std::string file_html;
      std::string file;
      file =  info._path_phys + p_dirent[i]->d_name;
      struct stat st;
      if(stat(file.c_str(),&st) < 0)
      {
        continue;

      }
      std::string mtime;
      Utils::TimeToGMT(st.st_mtime,mtime);
      std::string mime;
      Utils::GetMime(p_dirent[i]->d_name,mime);
      std::string fsize;
      Utils::DigitToStr(st.st_size/1024,fsize);
      file_html  += "<li><strong><a href='"+ info._path_info;
      file_html += p_dirent[i]->d_name;
      file_html += "'>" ;
      file_html += p_dirent[i]->d_name;
      file_html += "</a></strong>";
      file_html += "<br />";
      file_html += "<modified: " + mtime + "<br />";
      file_html +=  mime + " - " + fsize + "kbytes";
      file_html += "<br /><br /></small></li>";
      SendCData(file_html);
    }

    rsp_body = "</ol><hr /></body></html>";
    SendCData(rsp_body);
    SendCData("");

    return true;
  }
  bool ProcessCGI(RequestInfo &info)
  {
    //使用外部程序玩擦恒CGI请求处理--文件上传
    //将http头信息和正文全部交给子进程处理
    //使用环境变量传递头信息
    //使用管道传递正文数据
    //使用管道接受CGI程序的处理结果
    //流程：创建管道，创建子进程，设置子进程环境变量，程序替换
    int in[2];//用于向子进程传递数据
    int out[2];//用于从子进程中读取结果
    if(pipe(in)||pipe(out))
    {
      info._err_code = "500";
      ErrHandler(info);
      return false;
    }
    int pid = fork();
    if(pid < 0)
    {
      info._err_code = "500";
      ErrHandler(info);
      return false;
    }
    else if(pid == 0)
    {
      close(in[1]);
      close(out[0]);
      setenv("METHOD",info._method.c_str(),1);
      setenv("VERSION",info._version.c_str(),1);
      setenv("PATH_INFO",info._path_info.c_str(),1);
      setenv("QUERY_STRING",info._query_string.c_str(),1);
      for(auto it = info._hdr_list.begin(); it != info._hdr_list.end();it++)
      {
        setenv(it->first.c_str(),it->second.c_str(),1);
      }
      dup2(in[0],0);
      dup2(out[1],1);  // 将输出端重定向到out[1];子进程直接打印处理结果传递给父进程

      execl(info._path_phys.c_str(),info._path_phys.c_str(),NULL);
      exit(0);
    }
    close(in[0]);
    close(out[1]);
    //走下来的是父进程
    //通过in管道将正文数传递给子进程
    //通过out管道读取子进程的处理结果直到返回9
    //将处理结果组织http数据，响应给客户端
    auto it = info._hdr_list.find("Content-Length");
    if(it != info._hdr_list.end())
    {
      char buf[MAX_BUFF] = {0};
  
      int64_t content_len = Utils::StrToDigit(it->second);
      int tlen = 0;
      while(tlen < content_len)
      {
        int len =  MAX_BUFF > (content_len - tlen)?(content_len - tlen): MAX_BUFF;
      int rlen = recv(_cli_sock,buf,MAX_BUFF,0);
      if(rlen <= 0)
      {
        return false;
      }
      if( write(in[1],buf,MAX_BUFF)< 0)
      {
        return false;
      }
       tlen += rlen;
     }
    }
    std::string rsp_header;
    rsp_header = info._version + " " + "200 OK \r\n";
    rsp_header += "Content-Type: text/html\r\n";
    rsp_header += "Connection: close\r\n";
    rsp_header += "ETag: " + _etag + "\r\n";
    rsp_header += "Last-Modified: " + _mtime + "\r\n";
    rsp_header += "Date: " + _date + "\r\n\r\n";
    SendData(rsp_header);
    while(1)
    {
      char buf[MAX_BUFF] = {0};
      int rlen = read(out[0],buf,MAX_BUFF);
      if(rlen == 0)
        break;
      send(_cli_sock,buf,rlen,0);
    }
    close(in[1]);
    close(out[0]);
    return true;
  }
  bool CGIHandler(RequestInfo &info)
  {
    InitResponse(info);
    ProcessCGI(info);
    return true;
  }
  bool FileIsDir(RequestInfo &info)
  {
    if(info._st.st_mode & S_IFDIR)
    {
      if(info._path_info[info._path_info.length()-1] != '/')
        info._path_info.push_back('/');
      if(info._path_phys[info._path_phys.length()-1] != '/')
        info._path_phys.push_back('/');
      return true;
    }
    return false;
  }
  bool FileHandler(RequestInfo &info)
  {
    InitResponse(info); //初始化文件响应信息
    if(FileIsDir(info))// 判断请求文件是否为目录
    {
      ProcessList(info);//执行文件列表展示响应
    }
    else
    {
      ProcessFile(info);//执行文件下载响应
    }
    return true;
  }
  bool ErrHandler(RequestInfo &info)
  {
    //首行 协议版本 状态码 状态描述\r\n
    //头部content-length Date
    //空行 
    //正文 body = "<html><body><h1>404; <h1></body></html>"

    time_t t= time(NULL);
    std::string gmt;
    Utils::TimeToGMT(t,gmt);

    std::string rsp_body;
    rsp_body = "<html><body><h1>"+ info._err_code ;
    rsp_body += "<h1></body></html>";
    std::string cont_len;
    Utils::DigitToStr(rsp_body.length(),cont_len);

    std::string rsp_header; 
    rsp_header = info._version + " " + info._err_code + " ";
    rsp_header += Utils::GetErrDesc(info._err_code)+ "\r\n";
    rsp_header += "Date: "+ gmt + "\r\n";
    rsp_header += "Content-Length: " + cont_len + "\r\n\r\n"; 
    send(_cli_sock,rsp_header.c_str(),rsp_header.length(),0);
    send(_cli_sock,rsp_body.c_str(),rsp_body.length(),0);

    return true;
  }
};

