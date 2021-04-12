#ifndef PORT
#define PORT 59206
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "crc16.h"
#include "xmodemserver.h"
#include "helper.h"

static int server;
struct client *top = NULL; //represents top of client chain

static void addclient(int soc) 
{
  struct client *sender = malloc(sizeof(struct client));
  if (!soc)
  {
    fprintf(stderr, "out of memory!\n"); /* highly unlikely to happen */
    exit(1);
  }
  fflush(stdout);
  //havent read anything form client yet so buffer array and blocksize remains uninitialized
  //when connection is being established, the server is in the initial state
  //we know when reading that we will start at the first block so set current_block to 1
  //initialize inbuf to 0 as we haven't started to index buffer yet
  //when first connection is added make it at top of linked list
  //need to take top in as parameter to that we know if there are other existing connections and we have added to file correctly
  sender->fd = soc;
  sender->fp = NULL;
  sender->inbuf = 0;
  sender->state = initial;
  sender->next = top;
  sender->current_block = 0;
  top = sender;
}

static void removeclient(int soc) 
{
  struct client **sender;
  for (sender = (struct client **)&top; *sender && (*sender)->fd != soc; sender = &(*sender)->next)
    ;
  if (*sender)
  {
    struct client *t = (*sender)->next;
    if ((*sender)->fp)
    { //close file
      if (fclose((*sender)->fp) < 0)
      {
        perror("Error while closing open file for server");
        exit(1);
      }
    }
    fflush(stdout);
    free(*sender);
    *sender = t;
  }
  else
  {
    fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", soc);
    fflush(stderr);
  }
}

void newconnection()
{
  int soc;

  if ((soc = accept(server, NULL, NULL)) < 0)
  {
    perror("accept");
  }
  else
  {
    addclient(soc);
    printf("Client %d Added: \n", soc);
  }
}

void bindandlisten() /* bind and listen, abort on error */
{
  struct sockaddr_in reciever;

  if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("socket");
    exit(1);
  }

  memset(&reciever, '\0', sizeof(reciever));
  reciever.sin_family = AF_INET;
  reciever.sin_addr.s_addr = INADDR_ANY;
  reciever.sin_port = htons(PORT);

  if (bind(server, (struct sockaddr *)&reciever, sizeof reciever) < 0)
  {
    perror("bind");
    exit(1);
  }

  if (listen(server, 5) < 0)
  {
    perror("listen");
    exit(1);
  }
  int yes = 1;
  if ((setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1)
  {
    perror("setsockopt");
  }
}

int main(int argc, char *argv[])
{
  struct client *client;
  bindandlisten();
  printf("Server binded: \n");

  while (1)
  {
    //create set of file descriptors to store all clients
    fd_set client_set;
    //set maximum client to server as server is out first client
    int max_fd = server;
    //set client to 0
    char send[1];
    FD_ZERO(&client_set);
    //set server(listenfd) into our list of file descriptors
    FD_SET(server, &client_set);
    //go though linked lists of clients and set each one into client set
    for (client = top; client; client = client->next)
    {
      FD_SET(client->fd, &client_set);
      if (client->fd > max_fd)
      {
        max_fd = client->fd;
      }
    }
    //select the client to perform action on
    if (select(max_fd + 1, &client_set, NULL, NULL, NULL) < 0)
    {
      perror("select");
    }
    else
    {
      //if select was succefful that means there is some action required on client
      //check which client has been set and assign that client to client variable
      for (client = top; client; client = client->next)
      {
        if (FD_ISSET(client->fd, &client_set))
        {
          break;
        }
      }
      //if server is set in client set then initialize a new connection to the client
      if (FD_ISSET(server, &client_set) || !client)
      {
        newconnection();
      }
      //if the client has a value in it then check which state the client is in and then perform necessary action
      if (client)
      {
        if (client->state == initial)
        {
          //get filename from client
          //once complete file name is recieved check for network tags and remove them
          //name can be sent in multiple command so only switch state once full name has been read
          //send c to client once name has been read
          //change state to preblock
          // to deal with edge case store given parts of file name into buffer
          //inbuf keeps track of how many characters have been entered
          int bytes_read = read(client->fd, client->buf + client->inbuf, 21 - client->inbuf);
          if (bytes_read < 0)
          {
            printf("Error reading filename");
            client->state = finished;
          }
          client->inbuf += bytes_read;
          //check if network tags are in filename

          if (strstr(client->buf, "\r\n"))
          {
            //change network tags to null terminator
            //copy filename into client.filename
            strncpy(client->filename, client->buf, client->inbuf - 2);
            //if (strlen(client->filename))
            client->filename[client->inbuf - 2] = '\0';
            client->fp = open_file_in_dir(client->filename, "filestore");
            //reset inbuf and buffer
            char buffer[2048] = {0};
            strncpy(client->buf, buffer, client->inbuf);
            client->inbuf = 0;
            //set state to preblock
            client->state = pre_block;
            //send C to client
            send[0] = 'C';
            write(client->fd, &send, sizeof(char));
          }
          else if (client->inbuf == 21)
          {
            printf("Filename too long. \n");
            client->state = finished;
          }
        }
        if (client->state == pre_block)
        {
          //read character from client
          //if character is EOT then send ACK command and drop client
          //if character is SOH then set block size to 132 and switch to get_block
          //if character is STX then set blocksize to 1028 and switch to get_clock state
          //else ascii character enetred is invalid
          char command;
          read(client->fd, &command, sizeof(char));
          if (command == EOT)
          {
            send[0] = ACK;
            write(client->fd, &send, sizeof(char));
            client->state = finished;
          }
          else if (command == SOH)
          {
            client->blocksize = 132;
            client->state = get_block;
          }
          else if (command == STX)
          {
            client->blocksize = 1028;
            client->state = get_block;
          }
          else
          {
            printf("Invalid ASCII Charcter entered. \n");
            client->state = finished;
          }
        }
        if (client->state == get_block)
        {
          //read number of bytes assigned by pre_block
          //once sufficent bytes are alloctaed then transition to check_block
          //check if inbuf == block_size before transitioning because that means full block has been read
          //if not eveyrthing has been read remain in get_block state
          int bytes_read = read(client->fd, client->buf + client->inbuf, client->blocksize - client->inbuf);
          if (bytes_read == 0)
          {
            client->state = finished;
          }
          client->inbuf += bytes_read;
          if (client->inbuf == client->blocksize)
          {
            client->state = check_block;
            client->inbuf = 0;
          }
        }
        if (client->state == check_block)
        {
          //get block number, inverse, payload, crc16
          //check if block number and inverse correspond, if they do transition to finished
          //check if block number is the same as the previous block number then send an ACK command
          //if block number isnt one thats supposed to be next them transition to finish
          //keep in mind that after hitting block 255 the next block is supposed to be 0
          //check if high bytes and low bytes for crc16 work
          unsigned char block_number = client->buf[0];
          unsigned char inverse = client->buf[1];
          unsigned char crc_high = (client->buf[client->blocksize - 2]) & 0xFF;
          unsigned char crc_low = (client->buf[client->blocksize - 1]) & 0xFF;
          unsigned short crc_msg = crc_message(XMODEM_KEY, (unsigned char *)&(client->buf[2]), client->blocksize - 4);
          unsigned char crc_msg_high = (crc_msg >> 8) & 0xFF;
          if ((block_number != 255 - inverse))
          {
            printf("Block number does not correspond to inverse. \n");
            client->state = finished;
          }
          else if (block_number == client->current_block)
          {
            send[0] = ACK;
            write(client->fd, &send, sizeof(char));
          }
          else if ((block_number != client->current_block + 1 && client->current_block != 255) || (client->current_block == 255 && block_number != 0))
          {
            printf("Given block number is wrong. \n");
            client->state = finished;
          }
          else if (crc_high != crc_msg_high || crc_low != (crc_msg & 0xFF))
          {
            send[0] = NAK;
            write(client->fd, &send, sizeof(char));
          }
          else
          {
            fwrite(client->buf + 2, sizeof(char), client->blocksize - 4, client->fp);
            send[0] = ACK;
            write(client->fd, &send, sizeof(char));
            client->current_block = block_number;
            client->state = pre_block;
            client->inbuf = 0;
          }
        }
        else if (client->state == finished)
        {
          printf("Client %d dropped: \n", client->fd);
          close(client->fd);
          removeclient(client->fd);
        }
      }
    }
  }
}
