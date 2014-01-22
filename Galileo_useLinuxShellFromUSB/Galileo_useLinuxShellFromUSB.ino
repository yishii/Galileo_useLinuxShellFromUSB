/*
  ピンジャックタイプのRS-232Cケーブルを使用せず、GalileoのUSBデバイスポートからLinuxシェルを操作するスケッチ
  To do shell operation from Galileo's USB serial port without RS-232C pin-jack cable.
  
  テスト段階版です!
  This sketch is still under testing!
  
  Author ; Yasuhiro ISHII 1/23/2014
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/wait.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

typedef struct S_SENDER_PARAMS {
  int child_stdin_pipe;
} 
SenderParams;

void* parent_sender_thread(void* args)
{
  SenderParams* params = (SenderParams*)args;
  char data;

  while(1){
    pthread_testcancel();
    if(Serial.available() > 0){
      data = Serial.read();
      if(data == '\r'){
        data = '\n';
      }
      //Serial.print(data,HEX);
#if 0 // for Local echo back
      if(data == '\n'){
        Serial.write("\r\n");
      } else {
        Serial.write(data);
      }
#endif
      write(params->child_stdin_pipe,&data,1);
    }
  }
}

int runExternalCommand(const char* cmd, char* const args[], char* const envs[]) {
  int stdinPipe[2];
  int stdoutPipe[2];
  int pid;
  char data;
  int status;
  pthread_t thread;
  SenderParams senderparams;
  int result;

  if (pipe(stdinPipe) < 0) {
    return -1;
  }
  if (pipe(stdoutPipe) < 0) {
    close(stdinPipe[PIPE_READ]);
    close(stdinPipe[PIPE_WRITE]);
    return -1;
  }

  pid = fork();
  if (pid == 0) {
    // Child
    
    if (dup2(stdinPipe[PIPE_READ], STDIN_FILENO) == -1 ||
      dup2(stdoutPipe[PIPE_WRITE], STDOUT_FILENO) == -1 ||
      dup2(stdoutPipe[PIPE_WRITE], STDERR_FILENO) == -1){
      exit(errno); 
    }

    close(stdinPipe[PIPE_READ]);
    close(stdinPipe[PIPE_WRITE]);
    close(stdoutPipe[PIPE_READ]);
    close(stdoutPipe[PIPE_WRITE]); 

    result = execve(cmd, args, envs);

    Serial.println("Child process execution error");
    exit(result);
  } 
  else if (pid > 0) {
    // Parent

    close(stdinPipe[PIPE_READ]);
    close(stdoutPipe[PIPE_WRITE]); 

    senderparams.child_stdin_pipe = stdinPipe[PIPE_WRITE];
    pthread_create(&thread,NULL,parent_sender_thread,(void*)&senderparams);

    while (read(stdoutPipe[PIPE_READ], &data, 1) == 1) {
      if(data == '\n'){
        Serial.write("\r\n");
      } else {
        Serial.write(data);
      }
    }

    waitpid(pid,&status,0);
    pthread_cancel(thread);
    pthread_join(thread,NULL);
    Serial.println("Child process exited");

    sleep(1);

    close(stdinPipe[PIPE_WRITE]);
    close(stdoutPipe[PIPE_READ]);
  } 
  else {
    close(stdinPipe[PIPE_READ]);
    close(stdinPipe[PIPE_WRITE]);
    close(stdoutPipe[PIPE_READ]);
    close(stdoutPipe[PIPE_WRITE]);
  }
  return pid;
}

void setup()
{
  Serial.begin(115200);
  pinMode(13,OUTPUT);
  
  char* const args[] = {
    "/bin/bash",
    "-i",
    NULL
  };
  while(1){
    Serial.println("Starting shell...");
    // execute bash in interactive mode
    runExternalCommand("/bin/bash",args,NULL);
  }
}

void loop() {
  sleep(1);
}

