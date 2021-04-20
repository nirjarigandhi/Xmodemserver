# Xmodemserver
Write a server for a file transfer protocol called xmodem. When finished, you will be able to connect an xmodem client to your server and send a file to your server using the xmodem protocol.

**File Transfer Protocols**

A file transfer protocol is an agreement between sender and receiver for transferring a file across a (possibly unreliable) connection. You use HTTP or HTTPS to receive files every day when visiting webpages, and you may be familiar with FTP which is another popular protocol for file transfers.

In this assignment, you'll be writing a server for a file transfer protocol called xmodem. When finished, you will be able to connect an xmodem client to your server and send a file to your server using the xmodem protocol.

**Your Ports**

To avoid port conflicts, we're asking you to use the following number as the port on which your server will listen: take the last four digits of your student number, and add a 5 in front. For example, if your student number is 1007123456, your port would be 53456.

To have the OS release your server's port as soon as your server terminates, you should put the following socket option in your server code (where listenfd is the server's listening socket):
    
    
    int yes = 1;
    
    if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    
If it turns out that you still find a port in use, you may add 1 to your port number as necessary in order to use new ports (for example, the fictitious student here could also use 53457, 53458, 53459, 53460). When you shutdown your server (e.g. to compile and run it again), the OS may not release the old port immediately, so you may have to cycle through ports a bit.

**What is xmodem?**

xmodem was a popular protocol for sending and receiving files back when the majority of consumer networking occurred over phone lines using dial-up modems. It was possible for bursts of data to be garbled or lost completely because the connection was unreliable and subject to line noise. Unchecked, such garbled data would prevent you from receiving an exact copy of a file being sent to you! xmodem was introduced as a simple protocol for more reliable transfers. When the receiver detects data corruption, it signals to the sender to re-send the current packet so that each packet is received cleanly.

There is a lot of xmodem information out there. You can Read More, but there are a confusing number of versions of the protocol in use. Use only what is in this handout.

**CRC16**

CRC16 is an algorithm for detecting transmission errors in a file sent from a sender to a receiver. A CRC16 of a message is 16 bits (2 bytes) long. The sender of a message sends the message concatenated with its CRC16. When received, the receiver calculates its own CRC16 on the message and checks that its CRC16 matches what the sender said it should be. If it matches, it is likely that there was no transmission error. If the CRC16 doesn't match, the receiver can request that the sender re-send the message.

The xmodem protocol uses CRC16 to detect data corruption. We have provided an implementation of CRC16 for you to use in crc16.c.

**Operation of the Client**

In this assignment, there is an xmodem client and an xmodem server. The client sends files; the server receives and stores them. You are writing the server; we are providing one sample client and your server must work with this client and other clients that satisfy the protocol.

The behaviour of an xmodem client (i.e. a sender) is governed by a state machine. In the client1.c sample, there are five states, each of which has associated actions and transitions to other states. The client starts in the initial state. We briefly describe the states here. This is important information for you because your server is the "other half" of this protocol and you'll want to understand what the client is doing.

-```initial:``` in this state, the client is just beginning. It sends the filename to the server followed by a network newline. No null character is sent to terminate the filename. This state also sets the current block to 1, and then moves to the handshake state. (Notice here that the first block the sender sends has block number 1, not 0.)

-```handshake:``` in this state, the sender reads one character at a time from the socket, discarding the characters until a capital C is received. This C means that the server is ready to start receiving xmodem blocks. Once the C is found, the sender moves to send_block.

-```send_block:``` the goal of this state is to send a single block (or packet) of data to the server. The block includes the payload, which is the actual content of the file that we want to transfer. However, this isn't all: xmodem doesn't simply send the payload by itself because then the server has no way of knowing whether the payload has been corrupted. So, the xmodem client sends some overhead with each block. Here is what the sender sends for each block:

      1. An ASCII symbol known as SOH (start of header). This is an ASCII control character that tells the server that a block is starting.
      
      2. The current block number, as a one-byte integer. This is not an ASCII code like 49 for the number 1, but is the integer representation of 1.
      
      3. The inverse of the current block number. This is 255 minus the block number. The server is going to make sure that the block number and the inverse agree; for example, if the block number is 15, then the server will expect the inverse to be 240.
      
      4. The actual payload. This is always 128 bytes. You might wonder what happens if the file being sent is not a multiple of 128 bytes. In that case, the final block is padded with another ASCII control character called SUB. So, every file that successfully transfers to the server will have 0 to 127 bytes of this padding.
      
      5. The high byte of the CRC16 of the payload.
      
      6. The low byte of the CRC16 of the payload.

There's one more state transition to mention here. If the file is at EOF, then instead of sending a block and moving to wait_reply, the client moves to finish.

-```wait_reply:``` in this state, the client is waiting for a reply from the server. Did the server receive the block correctly? With the proper block number, inverse, and CRC16? If yes, then the server will send the client an ACK (another of those ASCII control characters), and the client will increment the block number and go back to send_block to send the next block. If not, then the server sends the client a NAK and the client will have to re-send the current block, so the client goes to send_block but does not increment the block number.

-```finish:``` now the client has sent all of the blocks. The only thing left is to send an EOT (that's an end-of-transfer ASCII control code) and wait for a final ACK. When the client receives that final ACK, we know that the server has received the entire file. If the client's EOT gets NAK'd, then the client repeats by sending another EOT.
This is a well-behaved client. It always sends the proper block numbers and inverses, and always sends the correct CRC16. It is also a simplified client, because it does not take advantage of an xmodem feature that allows a block length of 1024 bytes. The server you write will work with this simple client, but also clients that send wrong blocks or wrong CRC16s or send 1024-byte blocks in addition to 128-byte blocks.

**Your Task: xmodem Server**

Your task is to implement an xmodem server as described in this section. The server allows clients to connect and send a file to the server using the xmodem protocol.
