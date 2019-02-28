#include "utils.hpp"
enum _boundry_type
{
  BOUNDRY_NO = 0,
  BOUNDRY_FIRST,
  BOUNDRY_MIDDLE,
  BOUNDRY_LAST,
  BOUNDRY_BAK
};

class UpLoad
{
  private:
    int _file_fd;
    std::string _f_boundry;
    std::string _m_boundry;
    std::string _l_boundry;
    int64_t content_len;
    std::string _file_name;
  private:
    int  MatchBoundry(char *buf,int blen,int *boundry_pos){
      //----boundry 
      //first_boundry :------boundry\r\n
      //middle_boundry:\r\n------boundry\r\n
      //last_boundry:\r\n------boundry--\r\n
      //从起始位置匹配first_boundry
      if(!memcmp(buf,_f_boundry.c_str(),_f_boundry.length()))
      {
        *boundry_pos = 0;
        return BOUNDRY_FIRST;
      }
      for(int i = 0; i < blen; i++)
      {
        //如果剩下的数据不够一个boundry？
        //如果字符串剩余长度大于boundry的长度，则全部匹配
        if((blen -i) > _m_boundry.length())
        {

          if(!memcmp(buf+i,_m_boundry.c_str(),_m_boundry.length()))
          {
            *boundry_pos = i;
            return BOUNDRY_MIDDLE;
          }
          else  if(!memcmp(buf+i,_l_boundry.c_str(),_l_boundry.length()))
          {
            *boundry_pos = i;
            return BOUNDRY_LAST;
         }
        }
        //否则，如果剩余长度小于boundry长度，防止半个boundry，进行部分匹配
        else 
        {
          int cmp_len = (blen-i) > _m_boundry.length()? _m_boundry.length() : (blen-i);
          if(!memcmp(buf+i,_m_boundry.c_str(),cmp_len))
          {
            *boundry_pos = i;
            return BOUNDRY_BAK;
          }
          if(!memcmp(buf+i,_l_boundry.c_str(),cmp_len))
          {
            *boundry_pos = i;
            return BOUNDRY_BAK;       
         }

        }
     }
      return BOUNDRY_NO; 

    }
    bool GetFileName(char *buf,int * content_pos)
    {
      char *ptr = NULL;
      ptr = strstr(buf, "\r\n\r\n");
      if(ptr == NULL)
      {
        *content_pos = 0;
        return false;
      }
      *content_pos = (ptr - buf)+4;
      std::string header;
      header.assign(buf, ptr - buf);

      std::string file_sep = "filename=\"";
      size_t pos = header.find(file_sep);
      if(pos == std::string ::npos)
        return false;
      std::string file;
       file = header.substr(pos + file_sep.length());
      pos = file.find("\"");
      if(pos == std::string::npos)
      {
        return false;
      }
      file.erase(pos);
      _file_name = WWWROOT;
      _file_name += "/"; 
      _file_name += file;

      fprintf(stderr, "upload file:[%s]\n", _file_name.c_str());
      return true;
    }
    bool CreateFile()
    {
      _file_fd = open(_file_name.c_str(),O_CREAT|O_WRONLY,0664);
      if(_file_fd < 0)
      {
        return false;
      }
      return false;
    }
    bool CloseFile()
    {
      if(_file_fd != -1)
      {
        close(_file_fd);
        _file_fd = -1;
      }
      return true;
    }
    bool WriteFile(char *buf, int len)
    {
      if(_file_fd != -1)
      {
        write(_file_fd,buf, len);
      }
      return true;
    }
  public:
    UpLoad():_file_fd(-1){

    }
    bool InitUpLoadInfo()
    {
      umask(0);
      char *ptr = getenv("Content-Length");
      if(ptr == NULL)
      {
        fprintf(stderr,"have no content-length!!\n");
        return false;
      }
      content_len = Utils::StrToDigit(ptr);
      ptr = getenv("Content-Type");
      if(ptr == NULL)
      {
        fprintf(stderr,"have no content-Type!!\n");
        return false;
      }
      std::string boundry_sep = "boundary=";
      std::string content_type = ptr;
      size_t pos = content_type.find(boundry_sep);
      if(pos == std::string::npos)
      {
        fprintf(stderr,"content type have no boundry!!\n");
        return false;
      }

      std::string boundry = content_type.substr(pos + boundry_sep.length());
      _f_boundry = "--" + boundry ;
      _m_boundry = "\r\n" + _f_boundry +"\r\n" ;
      _l_boundry = "\r\n" + _f_boundry +"--";

      return true;
    }
    bool ProcessUpLoad()
    {
      int64_t tlen = 0,blen = 0;

      char buf[MAX_BUFF];
      while(tlen <content_len)
      {
        int len = read(0,buf + blen,MAX_BUFF - blen);
        blen += len; //当前buff中数据的长度
        int boundry_pos,content_pos;

        int flag = MatchBoundry(buf, blen, &boundry_pos);
        if(flag == BOUNDRY_FIRST)
        {
          //1.从boundry头中获取文件名
          //2，若获取文件名成功，则创建文件，打开文件
          //3，将头信息从buf中移除，剩下的数据进行下一步匹配
          if (GetFileName(buf,&content_pos))
          {
            CreateFile();
            blen -= content_pos;
            memmove(buf,  buf + content_pos,blen);
          }
          else 
          {
            if(content_pos == 0)
              continue;
            blen -=  _f_boundry.length();
            memmove(buf, buf + boundry_pos,blen);
          }
          
        }
        while(1)
        {
          int  flag = MatchBoundry(buf,blen,&boundry_pos);
          if(flag != BOUNDRY_MIDDLE)
          {
            break;
          }
          //1.匹配middle_boundry成功
          //将boundry之前数据写入文件，将数据从buf中移除
          //看middle_boundry头中是否有文件名，以下雷同first_boundry
          WriteFile(buf, boundry_pos);
          CloseFile();
          blen -= boundry_pos;
          memmove(buf, buf + boundry_pos,blen);
          if (GetFileName(buf,&content_pos))
          {
            CreateFile();
            blen -= content_pos;
            memmove(buf,  buf + content_pos, blen);
          }
          else 
          {
            if(content_pos == 0)
              break;
            blen -= _m_boundry.length();
            memmove(buf, buf + _m_boundry.length(), blen);
          }
        }
        flag = MatchBoundry(buf,blen,&boundry_pos);                                            
        if(flag == BOUNDRY_LAST) 
        { 
          //last_boundry匹配成功
          //1.将boundry之前的数据写入文件
          //2，关闭文件
          WriteFile(buf, boundry_pos);
          CloseFile();
          return true; 

        }
        flag = MatchBoundry(buf,blen,&boundry_pos);                                            
        if(flag == BOUNDRY_BAK)
        {
          // 将类似boundry位置之前的数据写入文件
          // 移除之前的数据
          // 剩下的数据不动，重新接受数据，补全之后继续匹配
          WriteFile(buf, boundry_pos);
          blen -= boundry_pos;
          memmove(buf,buf + boundry_pos, blen);
        }
        flag = MatchBoundry(buf,blen,&boundry_pos);                                            
        if(flag == BOUNDRY_NO)
        {
          //将所有buf所有数据写入文件
          WriteFile(buf, blen);
          blen = 0;

        }
        tlen += len;
      }
      return false;


    }
};
int main()
{
  UpLoad upload;
  std::string rsp_body;  
  if (upload.InitUpLoadInfo() == false)
    return -1;
  if(upload.ProcessUpLoad() == false)
  {
    rsp_body = "<html><body><h1>FAILED!</h1></body></html>";
  }
  else 
  {
    rsp_body = "<html><body><h1>SUCESS!</h1></body></html>";
  }

  std::cout << rsp_body;
  fflush(stdout);
  return 0;
}
