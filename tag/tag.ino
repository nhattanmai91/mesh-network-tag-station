/*
This code for Tag. Temporarily, the ID of Tag is 'this_node'
*/


#include <avr/pgmspace.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include "nodeconfig.h"
#include "printf.h"

// This is for git version tracking.  Safe to ignore
#ifdef VERSION_H
#include "version.h"
#else
#define __TAG__ "1"
#endif

#define __NAME__ "Tag-01"

// nRF24L01(+) radio using the Getting Started board
RF24 radio(9,10);
RF24Network network(radio);

// Our node address
uint16_t this_node;

// Delay manager to send pings regularly
const unsigned long interval = 2000; // ms
unsigned long last_time_sent;

// Array of nodes we are aware of
const short max_active_nodes = 10;
uint16_t active_nodes[max_active_nodes];
short num_active_nodes = 0;
short next_ping_node_index = 0;

// Prototypes for functions to send & handle messages
bool send_T(uint16_t to);
bool send_N(uint16_t to);
void handle_T(RF24NetworkHeader& header);
void handle_N(RF24NetworkHeader& header);
void add_node(uint16_t node);

void setup(void)
{
  //
  // Print preamble
  //
  
  Serial.begin(57600);
  printf_begin();
  printf_P(PSTR("\n\rRF24Network/examples/meshping/\n\r"));
  printf_P(PSTR("VERSION: " __TAG__ "\n\r"));
  
  //
  // Pull node address out of eeprom 
  //

  // Which node are we?
  this_node = nodeconfig_read();

  //
  // Bring up the RF network
  //

  SPI.begin();
  radio.begin();
  network.begin(/*channel*/ 100, /*node address*/ this_node );
}

void loop(void)
{
  // Pump the network regularly
  network.update();

  // Is there anything ready for us?
  while ( network.available() )
  {
    // If so, take a look at it 
    RF24NetworkHeader header;
    network.peek(header);

    // Dispatch the message to the correct handler.
    switch (header.type)
    {
    case 'T':
      handle_T(header);
      break;
    case 'N':
      handle_N(header);
      break;
    default:
      printf_P(PSTR("*** WARNING *** Unknown message type %c\n\r"),header.type);
      network.read(header,0,0);
      break;
    };
  }

  // Send a ping to the next node every 'interval' ms
  unsigned long now = millis();
  if ( now - last_time_sent >= interval )
  {
    last_time_sent = now;

    // Who should we send to?
    // By default, send to base
    uint16_t to = 00;
    
    // Or if we have active nodes,
    if ( num_active_nodes )
    {
      // Send to the next active node
      to = active_nodes[next_ping_node_index++];
      
      // Have we rolled over?
      if ( next_ping_node_index > num_active_nodes )
      {
	// Next time start at the beginning
	next_ping_node_index = 0;

	// This time, send to node 00.
	to = 00;
      }
    }

    bool ok;

    // Normal nodes send a 'T' ping
    if ( this_node > 00 || to == 00 )
      ok = send_T(to);
    
    // Base node sends the current active nodes out
    else
      ok = send_N(to);

    // Notify us of the result
    if (ok)
    {
      printf_P(PSTR("%s|%lu: APP Send ok\n\r"), __NAME__, millis());
    }
    else
    {
      printf_P(PSTR("%s|%lu: APP Send failed\n\r"),__NAME__,millis());

      // Try sending at a different time next time
      last_time_sent -= 100;
    }
  }

  // Listen for a new node address
  nodeconfig_listen();
}

/**
 * Send a 'T' message, the current time
 */
bool send_T(uint16_t to)
{
  RF24NetworkHeader header(/*to node*/ to, /*type*/ 'T' /*Time*/);
  
  // The 'T' message that we send is just a ulong, containing the time
  unsigned long message = this_node;
  printf_P(PSTR("---------------------------------\n\r"));
  printf_P(PSTR("%s|%lu: APP Sending %lu to 0%o...\n\r"), __NAME__, millis(),message,to);
  return network.write(header,&message,sizeof(unsigned long));
}

/**
 * Send an 'N' message, the active node list
 */
bool send_N(uint16_t to)
{
  RF24NetworkHeader header(/*to node*/ to, /*type*/ 'N' /*Time*/);
  
  printf_P(PSTR("---------------------------------\n\r"));
  printf_P(PSTR("%s|%lu: APP Sending active nodes to 0%o...\n\r"),__NAME__,millis(),to);
  return network.write(header,active_nodes,sizeof(active_nodes));
}

/**
 * Handle a 'T' message
 *
 * Add the node to the list of active nodes
 */
void handle_T(RF24NetworkHeader& header)
{
  // The 'T' message is just a ulong, containing the time
  unsigned long message;
  network.read(header,&message,sizeof(unsigned long));
  printf_P(PSTR("%s|%lu: APP Received %lu from 0%o\n\r"),__NAME__,millis(),message,header.from_node);

  // If this message is from ourselves or the base, don't bother adding it to the active nodes.
  if ( header.from_node != this_node || header.from_node > 00 )
    add_node(header.from_node);
}

/**
 * Handle an 'N' message, the active node list
 */
void handle_N(RF24NetworkHeader& header)
{
  static uint16_t incoming_nodes[max_active_nodes];

  network.read(header,&incoming_nodes,sizeof(incoming_nodes));
  printf_P(PSTR("%s|%lu: APP Received nodes from 0%o\n\r"),__NAME__,millis(),header.from_node);

  int i = 0;
  while ( i < max_active_nodes && incoming_nodes[i] > 00 )
    add_node(incoming_nodes[i++]);
}

/**
 * Add a particular node to the current list of active nodes
 */
void add_node(uint16_t node)
{
  // Do we already know about this node?
  short i = num_active_nodes;
  while (i--)
  {
    if ( active_nodes[i] == node )
      break;
  }
  // If not, add it to the table
  if ( i == -1 && num_active_nodes < max_active_nodes )
  {
    active_nodes[num_active_nodes++] = node; 
    printf_P(PSTR("%s|%lu: APP Added 0%o to list of active nodes.\n\r"),__NAME__,millis(),node);
  }
}

// vim:ai:cin:sts=2 sw=2 ft=cpp
