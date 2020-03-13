#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#define maxlength 256
#define bufsize 1024

typedef struct
{
  char name[maxlength];
  int size;
} Header;
//информация о файле в архиве
typedef struct
{
  char name[maxlength];
  char directory[maxlength];
  int size;
} Fileinfo;

Header header;
int fdArchive; //дискриптор архива
char path[maxlength];//запись о расположении файла в архиве
void Pack(char* directory);//архивирование папки
void ArchiveFile(char* filename);//архивирование файла
void Unpack(char* archive,char* directory);//разархивирование
void Add(char* archive,char* filename);//добавление в архив

int main(int argc, char**argv)
{
  int minNumberOfargs=2;
  int pack=0;
  int currentDir=0;
  int unpack=0;
  int add=0;
  char c=0;

  while (--argc>0 && (*++argv)[0]=='-')
  {
    while (c=*++argv[0])
    {
      switch (c)
      {
        case 'p':
              if (pack)
                continue;
              if (add | currentDir |unpack)
              {
                printf("only one of the flags '-p' '-a' -u[c]' can be used\n");
                exit(1);
              }
              pack=1;
              break;

        case 'c':
              if (currentDir)
                  continue;
              if (add|pack)
              {
                printf("only one of the flags '-p' '-a' -u[c]' can be used\n");
                exit(1);
              }
              currentDir=1;
              minNumberOfargs--;
              break;

        case 'u':
              if (unpack)
                continue;
              if (add|pack)
              {
                printf("only one of the flags '-p' '-a' -u[c]' can be used\n");
                exit(1);
              }
              unpack=1;
              break;

        case 'a':
              if (add)
                continue;
              if (pack|unpack|currentDir)
              {
                printf("only one of the flags '-p' '-a' -u[c]' can be used\n");
                exit(1);
              }
              add=1;
              break;
        default:
              printf("Wrong flags\n");
              exit(1);
              break;
      }
    }
  }

  if (argc<minNumberOfargs)
  {
    printf("Wrong format\n");
    exit(1);
  }

  char current_directory[maxlength];
  char* end;// указатель для разделения директории и файла
  getcwd(current_directory,maxlength);

  if (pack)
  {
    if ((fdArchive=creat(*argv,S_IRUSR|S_IWUSR))==0)
      {
        fprintf(stderr,"cannot create file: %s\n",*argv);
        perror("ERROR:");
        exit(1);
      }
    lseek(fdArchive,sizeof(header),SEEK_SET);
    struct stat statbuf;

    while (--argc>0)
    {
      ++argv;
      stat(*argv,&statbuf);
      if (S_ISDIR(statbuf.st_mode))//если каталог
      {
        strcpy(path,*argv);
        Pack(*argv);
      }
      else
      {
        strcpy(path,*argv);
        if((end=strrchr(path,'/'))!=NULL)
        {
          *end='\0';
          chdir(path);
          ArchiveFile(end+1);
          chdir(current_directory);
        }
        else
        {
          path[0]='\0';
          ArchiveFile(*argv);
        }
      }
    }
    lseek(fdArchive,0,SEEK_SET);
    write(fdArchive,&header,sizeof(header));//запись заголовка
    close(fdArchive);
    exit(0);
  }

  if (unpack)
  {
    if ((fdArchive=open(*argv,O_RDONLY))==0)
      {
        fprintf(stderr,"cannot open file: %s\n",*argv);
        perror("ERROR:");
        exit(1);
      }
    currentDir ? Unpack(*argv,NULL):Unpack(*argv,*(argv+1));
    close(fdArchive);
    exit(0);
  }

  if (add)
  {
    if ((fdArchive=open(*argv,O_RDWR))==0)
    {
      fprintf(stderr,"cannot open file: %s\n",*argv);
      perror("ERROR:");
      exit(1);
    }
    char archive[maxlength];
    strcpy(archive,*argv);
    while (--argc>0)
    {
      Add(archive,*++argv);
      lseek(fdArchive,0,SEEK_SET);
    }

    close(fdArchive);
    exit(0);
  }
  exit(0);
}

void Pack (char* directory)
{
  DIR * dp;
  struct dirent *entry;
  struct stat statbuf;
  int fullDir=0;

  if((dp=opendir(directory))==NULL)
  {
    fprintf(stderr, "cannot open directory: %s\n",directory);
    perror("ERROR:");
    exit(1);
  }
  chdir(directory);
  while ((entry=readdir(dp))!=NULL)
  {
    lstat(entry->d_name, &statbuf);
    if (S_ISDIR(statbuf.st_mode))
    {
      if (strcmp(".",entry->d_name)==0 ||
          strcmp("..",entry->d_name)==0)
          continue;
      fullDir=1;
      strcpy(path+strlen(path),"/");
      strcpy(path+strlen(path),entry->d_name);
      Pack(entry->d_name);
    }
    else
    {
      ArchiveFile(entry->d_name);
      fullDir=1;
    }
  }

  if (!fullDir)//если архивируется пустая папка
  {
    Fileinfo fileinfo;
    header.size+=sizeof(fileinfo);
    fileinfo.name[0]='\0';
    memcpy(fileinfo.directory,path,strlen(path)+1);
    fileinfo.size=0;
    write(fdArchive,&fileinfo,sizeof(fileinfo));
  }
  char* end;
  if((end=strrchr(path,'/'))!=NULL)
  *end='\0';
  chdir("..");
  closedir(dp);
}

void ArchiveFile(char* filename)
{
  int fd;
  Fileinfo fileinfo;
  char buf[bufsize];
  struct stat statbuf;
  if ((fd=open(filename,O_RDONLY))==0)
    {
      fprintf(stderr,"cannot open file: %s\n",filename);
      exit(1);
    }
  header.size+=sizeof(fileinfo);
  memcpy(fileinfo.name,filename,strlen(filename)+1);//запись имени архивируемого файла
  memcpy(fileinfo.directory,path,strlen(path)+1);//запись директории архивируемого файла
  fstat(fd,&statbuf);
  fileinfo.size=statbuf.st_size;//запись размера
  write(fdArchive,&fileinfo,sizeof(fileinfo));
  int nread;
  while ((nread=read(fd,buf,sizeof(buf)))>0)//запись содержимого файла в архив
  {
    header.size+=nread;
    write(fdArchive,buf,nread);
  }
  close(fd);
}

void Unpack(char* archive,char* directory)
{
  char *end;
  char unpackDir[maxlength];
  struct stat statbuf;
  if((end=strrchr(archive,'/'))!=NULL)//выделение имени архива
    end++;
  else
    end=archive;

  if (directory!=NULL) // составление директории для разархивирования
  {
    strcpy(unpackDir,directory);
    strcpy(unpackDir+strlen(unpackDir),"/");
    strcpy(unpackDir+strlen(unpackDir),end);
  }
  else
    strcpy(unpackDir,end);

  while (stat(unpackDir,&statbuf)==0)//если в директории уже есть файл с именем архива
  {
    int sizeDir=strlen(unpackDir);
    int check=0;
    if (unpackDir[sizeDir-1]==')')
    {
      int i;
      for (i=sizeDir-2;i>=0;--i)
      {
        if(unpackDir[i]=='(')
        {
          check=1;
          break;
        }
          if(!isdigit(unpackDir[i]))
          {
            strcpy(unpackDir+sizeDir,"(1)");
            break;
          }

        }
        if(check)//если файл с таким номером уже есть
        {//увеличиваем номер файла
            char numberRecord[maxlength];
            unpackDir[sizeDir]='\0';
            sprintf(numberRecord,"%d",atoi(unpackDir+i+1)+1);
            strcpy(unpackDir+i+1,numberRecord);
            strcpy(unpackDir+i+1+strlen(numberRecord),")");
        }
    }
    else
      strcpy(unpackDir+sizeDir,"(1)");
  }

if (mkdir(unpackDir, S_IRWXU | S_IRWXG | S_IRWXO)==-1)
{
  fprintf(stderr,"cannot create directory: %s\n",unpackDir);
  perror("MKDIR ERROR ");
  exit(1);
}
chdir(unpackDir);
read(fdArchive,&header,sizeof(header));
int sizeArc=header.size;

Fileinfo fileinfo;
char entireDir[maxlength];
entireDir[0]='\0';
char unpackFile[maxlength];
unpackDir[0]='\0';
while (sizeArc>0)//пока архив не кончился
{
  read(fdArchive,&fileinfo,sizeof(fileinfo));
  sizeArc-=sizeof(fileinfo);
  int sizefile=fileinfo.size;
  if (stat(fileinfo.directory,&statbuf)!=0 && fileinfo.directory[0]!='\0')//если директория для файла не была ещё создана
  {
    char temp[maxlength];
    int pos=0;
    for (int i=0; entireDir[i]==fileinfo.directory[i];i++)//поиск совпадений между предыдущей и текущей иерархиями каталогов
    {
      switch (entireDir[i+1])
        {
          case '/' :
                pos=i+2;
                break;
          case '\0':
                pos=i+1;
                break;
        default:
                break;
          }
    }
    if(entireDir[pos]=='\0' && pos)//файл находится в предыдущей директории
    {
      strcpy(entireDir+pos,"/");
      pos++;
    }

    strcpy(temp,fileinfo.directory);//копирование директории во временную строку
    char* ptr=strtok(temp+pos,"/");//выбор каталога из места несовпадения путей
    while(1)
    {
      strcpy(entireDir+pos,ptr);//дополнение иерархии новым каталогом с места последнего совпадения
      if (stat(entireDir,&statbuf)!=0)//если каталога нет
        if (mkdir(entireDir, S_IRWXU | S_IRWXG | S_IRWXO)==-1)//создание каталога
        {
          fprintf(stderr,"cannot create directory: %s\n",entireDir);
          perror("MKDIR ERROR");
          exit(1);
        }
      pos=strlen(entireDir);
      if((ptr=strtok(NULL,"/"))==NULL)//получение следующего каталога
        break;
      strcpy(entireDir+pos,"/");
      pos++;
    }
  }

  if(fileinfo.name[0]=='\0')//файл будет сохранён в первой папке
    continue;
  unpackFile[0]='\0';
  if(fileinfo.directory[0]!='\0')
  {
    strcpy(unpackFile,fileinfo.directory);
    strcpy(unpackFile+strlen(unpackFile),"/");
  }
  strcpy(unpackFile+strlen(unpackFile),fileinfo.name);//полный путь файла для сохранения

  int fd;
  if ((fd=creat(unpackFile, S_IRWXU|S_IRGRP|S_IXGRP))==0)
  {
        fprintf(stderr,"cannot create file: %s\n",unpackFile);
        exit(1);
      }
      char buffer[bufsize];
      int nread;
      while(sizefile>0)
      {
        if(sizefile<bufsize)
        {
          nread=read(fdArchive,buffer,sizefile);
          sizefile=0;
        }
        else
        {
          nread=read(fdArchive,buffer,bufsize);
          sizefile-=nread;
        }
        write(fd,buffer,nread);
        sizeArc-=nread;
      }
    close(fd);
  }
}

void Add(char* archive,char* filename)
{
  struct stat statbuf;
  char* end;
  char shortname[maxlength];//имя файла без пути к нему
  char current_directory[maxlength];
  int isdir=0;
  int changedir=0;
  int same=0;
  Fileinfo fileinfo;

  if(stat(filename,&statbuf)!=0)
  {
    fprintf(stderr,"file is not available: %s\n",filename);
    perror("error: ");
    exit(1);
  }
  if (S_ISDIR(statbuf.st_mode))
      isdir=1;

  read(fdArchive,&header,sizeof(header));
  int size= header.size;//получаем размер архива

  if((end=strrchr(filename,'/'))!=NULL)
  {
    *end='\0';
    end++;
    changedir=1;
    getcwd(current_directory,maxlength);
  }
  else
    end=filename;

  strcpy(shortname,end);//сохранение имени файла без пути к нему
  int len=strlen(shortname);

  while (size>0)//пока архив не закончился
  {
    read(fdArchive,&fileinfo,sizeof(fileinfo));
    size-=sizeof(fileinfo);
    if (isdir)//если добавляем директорию
    {
      if(strncmp(fileinfo.directory,shortname,len)==0 &&
       (fileinfo.directory[len]=='/' || fileinfo.directory[len]=='\0'))//в архиве есть директория с таким же именем
       {
         same=1;
         break;
       }
    }
    else//если добавляем файл
    {
      if(strcmp(fileinfo.name,shortname)==0)//в архиве есть файл с таким же именем
       {
         same=1;
         break;
       }
    }
    size-=fileinfo.size;
    lseek(fdArchive,fileinfo.size,SEEK_CUR);
  }

  if (same)//если в архиве есть файл или директория с таким же именем
  {
    int fd;
    char buf[bufsize];
    char temp[]="temp_fileXXXXXX";
    if ((fd=mkstemp(temp))==0)//создание временнного файла
    {
      fprintf(stderr,"cannot create temporary file\n");
      perror("ERROR:");
      exit(1);
    }

    lseek(fdArchive,sizeof(header),SEEK_SET);
    lseek(fd,sizeof(header),SEEK_SET);
    int nread=0;
    size=header.size;
    while (size>0)// запись в промежуточный файл содержимое архива, кроме повторяющегося файла
    {
      read(fdArchive,&fileinfo,sizeof(fileinfo));
      size-=sizeof(fileinfo);
      if (isdir)//пропуск повторяющихся файлов
      {
        size-=sizeof(fileinfo);
        if(strncmp(fileinfo.directory,shortname,len)==0 &&
         (fileinfo.directory[len]=='/' || fileinfo.directory[len]=='\0'))
         {
           size-=fileinfo.size;
           lseek(fdArchive,fileinfo.size,SEEK_CUR);
           header.size-=fileinfo.size;
           header.size-=sizeof(fileinfo);
           continue;
         }
      }
      else
      {
        if(strcmp(fileinfo.name,shortname)==0)
         {
           size-=fileinfo.size;
           lseek(fdArchive,fileinfo.size,SEEK_CUR);
           header.size-=fileinfo.size;
           header.size-=sizeof(fileinfo);
           continue;
         }
      }

      write(fd,&fileinfo,sizeof(fileinfo));
      int sizefile=fileinfo.size;

      while(sizefile>0)//запись во временнный файл
      {

        if(sizefile<sizeof(buf))
        {
          nread=read(fdArchive,buf,sizefile);
          sizefile=0;
        }
        else
        {
          nread=read(fdArchive,buf,sizeof(buf));
          sizefile-=nread;
        }
        write(fd,buf,nread);
        size-=nread;
      }

    }
    lseek(fd,0,SEEK_SET);
    write(fd,&header,sizeof(header));
    lseek(fd,0,SEEK_SET);
    close(fdArchive);

    if ((fdArchive=open(archive,O_RDWR | O_TRUNC))==0)//повторное открытие архива
    {
      fprintf(stderr,"cannot open file: %s\n",archive);
      exit(1);
    }

    while ((nread=read(fd,buf,sizeof(buf)))>0)//перехзапись данных из временного файла в архив
    {
      write(fdArchive,buf,nread);
    }

    if (unlink(temp))//удаление ссылки на временный файл
    {
      fprintf(stderr,"unlinking of %s was not successful\n",temp);
      perror("ERROR:");
      exit(1);
    }

  }
  lseek(fdArchive,0,SEEK_END);
  if(isdir)
  {
    strcpy(path,shortname);
    if (changedir)//если добавляемый файл не в текущей директории
    {
      chdir(filename);
      Pack(shortname);
      chdir(current_directory);
    }
    else
      Pack(shortname);
  }
  else
  {
    path[0]='\0';
    if (changedir)
    {
      chdir(filename);
      ArchiveFile(shortname);
      chdir(current_directory);
    }
    else
    {
      ArchiveFile(shortname);
    }
  }
  lseek(fdArchive,0,SEEK_SET);
  write(fdArchive,&header,sizeof(header));
}
