#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
  //open logging for syslog  
  openlog(/*ident=*/NULL,/*option*/0,/*facility=*/LOG_USER);
  if (argc != 3) {
    syslog(/*priority=*/LOG_ERR,/*format=*/"Expected 2 parameters, but found %d",argc-1);
    return 1;
  }
  char* writefile = argv[1]; // full path to a file (including filename)
  char* writestr = argv[2]; //text string which will be written within this file
  int fd = open(writefile, /*flags=*/O_WRONLY|O_CREAT|O_TRUNC);
  if (fd == -1) {
    syslog(/*priority=*/LOG_ERR,/*format=*/"Could not open file %s",writefile);
    return 1;
  }
  syslog(/*priority=*/LOG_DEBUG,/*format=*/"Writing %s to %s",writestr,writefile);
  ssize_t nr = write(fd,writestr,strlen(writestr));
  if (nr == -1) {
    syslog(/*priority=*/LOG_ERR,/*format=*/"Write failed");
    return 1;  
  }
  int ret = fsync(fd);
  if (ret == -1) {
    syslog(/*priority=*/LOG_ERR,/*format=*/"Sync file failed with ret %d",ret);
    return 1;
  }
  if (close(fd) == -1) {
    syslog(/*priority=*/LOG_ERR,/*format=*/"Closing file failed");
    return 1;
  }
  return 0; 
}

